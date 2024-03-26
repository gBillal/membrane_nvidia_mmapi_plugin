#include "decoder.h"

using namespace std;

#define MAX_FRAMES 16

#define TEST_ERROR(cond, fun, env, err) if(cond) { \
                                        res = fun(env, err); \
                                        goto cleanup; }

int qBuffer(State* state, UnifexPayload* payload, int64_t timestamp) {
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];

    NvBuffer* buffer = state->dec->output_plane.getNthBuffer(state->buf_idx);
    if (payload != NULL) {
        buffer->planes[0].data = payload->data;
        buffer->planes[0].bytesused = payload->size;
    } else {
        buffer->planes[0].bytesused = 0;
    }

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, sizeof(planes));

    v4l2_buf.index = state->buf_idx;
    v4l2_buf.m.planes = planes;
    v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
    
    v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
    v4l2_buf.timestamp.tv_sec = timestamp / (MICROSECOND_UNIT);
    v4l2_buf.timestamp.tv_usec = timestamp % (MICROSECOND_UNIT);

    state->buf_idx = (state->buf_idx + 1) % MAX_BUFFERS;

    return state->dec->output_plane.qBuffer(v4l2_buf, NULL);
}

int setCapturePlane(State* state) {
    NvVideoDecoder* dec = state->dec;
    struct v4l2_event event;
    int ret;

    do {
        if (dec->dqEvent(event, 1000) < 0)
            return ERR_DQ_EVENT;
    } while (event.type != V4L2_EVENT_RESOLUTION_CHANGE);

    v4l2_format format;
    v4l2_crop crop;
    NvBufSurf::NvCommonAllocateParams params;
    int32_t min_dec_capture_buffers;

    if (dec->capture_plane.getFormat(format) < 0) {
        return ERR_GET_FORMAT;
    }

    if (dec->capture_plane.getCrop(crop) < 0) {
        return ERR_GET_CROP;
    }

    state->width = state->width == -1 ? crop.c.width : state->width;
    state->height = state->height == -1 ? crop.c.height : state->height;

    params.memType = NVBUF_MEM_SURFACE_ARRAY;
    params.width = state->width;
    params.height = state->height;
    params.layout = NVBUF_LAYOUT_PITCH;
    params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;
    params.memtag = NvBufSurfaceTag_VIDEO_CONVERT;

    if (NvBufSurf::NvAllocate(&params, 1, &state->dst_dma_fd) < 0) {
        return ERR_NV_ALLOCATE;
    }

    dec->capture_plane.deinitPlane();
    ret = dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers);
    if (ret < 0) return ERR_GET_MIN_CAPTURE_PLANES;


    ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat, 
                                    format.fmt.pix_mp.width, 
                                    format.fmt.pix_mp.height);
    if (ret < 0) return ERR_SET_CAPTURE_PLANE_FORMAT;

    ret = dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP, min_dec_capture_buffers, false, false);
    if (ret < 0) return ERR_SETUP_PLANE;
    
    ret = dec->capture_plane.setStreamStatus(true);
    if (ret < 0) return ERR_SET_STREAM_STATUS;

    for (uint32_t i = 0; i < dec->capture_plane.getNumBuffers(); i++)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));

        v4l2_buf.index = i;
        v4l2_buf.m.planes = planes;
        v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_MMAP;
        if (dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0) {
            return ERR_CAPTURE_PLANE_QBuffer;
        }
    }

    state->wait_for_resolution_event = false;
    return 0;
}

int dmabufToUnifexPayload(int dmabuf_fd, unsigned int num_planes, UnifexPayload* payload) {
    if (dmabuf_fd <= 0)
        return -1;

    uint offset = 0;
    for (uint plane = 0; plane < num_planes; plane++) {
        int ret = -1;

        NvBufSurface *nvbuf_surf = 0;
        ret = NvBufSurfaceFromFd(dmabuf_fd, (void**)(&nvbuf_surf));
        if (ret != 0)
        {
            return -1;
        }

        ret = NvBufSurfaceMap(nvbuf_surf, 0, plane, NVBUF_MAP_READ_WRITE);
        if (ret < 0)
        {
            printf("NvBufSurfaceMap failed\n");
            return ret;
        }

        NvBufSurfaceSyncForCpu (nvbuf_surf, 0, plane);

        int row_size = nvbuf_surf->surfaceList->planeParams.width[plane] * nvbuf_surf->surfaceList->planeParams.bytesPerPix[plane];
        for (uint i = 0; i < nvbuf_surf->surfaceList->planeParams.height[plane]; ++i)
        {
            memcpy(payload->data + offset + i * row_size, 
                (char*)nvbuf_surf->surfaceList->mappedAddr.addr[plane] + i * nvbuf_surf->surfaceList->planeParams.pitch[plane],
                row_size
            );
        }
        offset += nvbuf_surf->surfaceList->planeParams.height[plane] 
            * nvbuf_surf->surfaceList->planeParams.width[plane] 
            * nvbuf_surf->surfaceList->planeParams.bytesPerPix[plane];

        ret = NvBufSurfaceUnMap(nvbuf_surf, 0, plane);
        if (ret < 0)
        {
            printf("NvBufSurfaceUnMap failed\n");
            return ret;
        }
    }

    return 0;
}

int getDecodedFrames(UnifexEnv *env, State* state, UnifexPayload*** ret_frames, int64_t** ret_pts, uint &total_frames) {
    int ret;
    bool full_output_plane = state->dec->output_plane.getNumQueuedBuffers() == MAX_BUFFERS;

    UnifexPayload** out_frames = (UnifexPayload**)unifex_alloc(sizeof(*out_frames) * MAX_FRAMES);
    int64_t* out_pts = (int64_t*)unifex_alloc(sizeof(*out_pts) * MAX_FRAMES);
    int frame_size = state->width * state->height + state->width * state->height / 2;

    while(true) {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        NvBuffer* buffer;

        memset(&v4l2_buf, 0, sizeof(v4l2_buf));
        memset(planes, 0, sizeof(planes));
        v4l2_buf.m.planes = planes;

        ret = state->dec->capture_plane.dqBuffer(v4l2_buf, &buffer, NULL, 10);
        if (errno == EAGAIN) {
            // If all the buffers are queued, we'll wait for the decoder
            // to decode at least one frame
            if (full_output_plane && total_frames == 0) continue;

            // If it's the end of stream, we'll wait for all the frames to be decoded
            if (state->eos && !(v4l2_buf.flags & V4L2_BUF_FLAG_LAST)) continue;

            break;
        } else if (ret < 0) return ERR_CAPTURE_PLANE_DQ_Buffer;

        NvBufSurf::NvCommonTransformParams transform_params;
        transform_params.flag = NVBUFSURF_TRANSFORM_FILTER;
        transform_params.flip = NvBufSurfTransform_None;
        transform_params.filter = NvBufSurfTransformInter_Nearest;
        
        ret = NvBufSurf::NvTransform(&transform_params, buffer->planes[0].fd, state->dst_dma_fd);
        if (ret < 0) return ERR_NV_TRANSFORM;

        ret = state->dec->capture_plane.qBuffer(v4l2_buf, NULL);
        if (ret < 0) return ERR_CAPTURE_PLANE_QBuffer;

        out_frames[total_frames] = (UnifexPayload*)unifex_alloc(sizeof(UnifexPayload));
        unifex_payload_alloc(env, UNIFEX_PAYLOAD_BINARY, frame_size, out_frames[total_frames]);
        dmabufToUnifexPayload(state->dst_dma_fd, 3, out_frames[total_frames]);
        out_pts[total_frames] = v4l2_buf.timestamp.tv_sec * MICROSECOND_UNIT + v4l2_buf.timestamp.tv_usec;
        total_frames++;
    }

    *ret_frames = out_frames;
    *ret_pts = out_pts;

    return 0;
}

int dqBuffer(State* state) {
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, sizeof(planes));

    v4l2_buf.m.planes = planes;

    return state->dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
}

UNIFEX_TERM create(UnifexEnv *env, char* pix_fmt, int width, int height) {
    UNIFEX_TERM res;
    int ret;
    State *state = unifex_alloc_state(env);

    state->dec = NvVideoDecoder::createVideoDecoder("dec0", O_NONBLOCK);
    TEST_ERROR(!state->dec, create_result_error, env, "open");

    ret = state->dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0);
    TEST_ERROR(ret < 0, create_result_error, env, "subscribe");

    int output_plane_pix_fmt;
    if (strcmp(pix_fmt, "H264") == 0) {
        output_plane_pix_fmt = V4L2_PIX_FMT_H264;
    } else {
        output_plane_pix_fmt = V4L2_PIX_FMT_H265;
    }

    ret = state->dec->setOutputPlaneFormat(output_plane_pix_fmt, CHUNK_SIZE);
    TEST_ERROR(ret < 0, create_result_error, env, "output_plane_format");

    ret = state->dec->setFrameInputMode(0);
    TEST_ERROR(ret < 0, create_result_error, env, "frame_input");

    // ret = state->dec->setSkipFrames(V4L2_SKIP_FRAMES_TYPE_DECODE_IDR_ONLY);
    // TEST_ERROR(ret < 0, create_result_error, env, "skip_frames");

    ret = state->dec->output_plane.setupPlane(V4L2_MEMORY_USERPTR, MAX_BUFFERS, false, true);
    TEST_ERROR(ret < 0, create_result_error, env, "setup_plane");

    ret = state->dec->output_plane.setStreamStatus(true);
    TEST_ERROR(ret < 0, create_result_error, env, "stream_status");

    state->wait_for_resolution_event = true;
    state->dst_dma_fd = -1;
    state->buf_idx = 0;
    state->width = width;
    state->height = height;
    state->eos = false;

    return create_result_ok(env, state);

cleanup:
    if (state->dec) delete state->dec;
    return res;
}

UNIFEX_TERM decode(UnifexEnv *env, UnifexPayload* payload, int64_t timestamp, State* state) {
    UNIFEX_TERM res;
    UnifexPayload** out_frames = NULL;
    int64_t* out_pts = NULL;
    uint total_frames = 0;

    if (qBuffer(state, payload, timestamp) < 0) {
        return decode_result_error(env, "qBuffer");
    }

    if (state->wait_for_resolution_event && setCapturePlane(state) < 0) {
        return decode_result_error(env, "set_capture_plane");
    }

    if (getDecodedFrames(env, state, &out_frames, &out_pts, total_frames) < 0) {
        return decode_result_error(env, "getDecodedFrames");
    }

    while(dqBuffer(state) == 0);

    res = decode_result_ok(env, out_frames, total_frames, out_pts, total_frames);
  
    if (out_frames != NULL) {
        for (uint i = 0; i < total_frames; i++) {
            unifex_payload_release(out_frames[i]);
            unifex_free(out_frames[i]);
        }
        unifex_free(out_frames);
        unifex_free(out_pts);
    }

    return res;
}

UNIFEX_TERM flush(UnifexEnv* env, State* state) {
    UNIFEX_TERM res;
    UnifexPayload** out_frames = NULL;
    int64_t* out_pts = NULL;
    uint total_frames = 0;
    int ret;

    state->eos = true;

    if (qBuffer(state, NULL, 0) < 0) {
        return decode_result_error(env, "qBuffer");
    }

    ret = getDecodedFrames(env, state, &out_frames, &out_pts, total_frames);
    if (ret < 0) {
        return decode_result_error(env, "getDecodedFrames");
    }

    ret = dqBuffer(state);
    if (ret < 0) {
        return decode_result_error(env, "dqBuffer");
    }

    res = decode_result_ok(env, out_frames, total_frames, out_pts, total_frames);
    
    if (out_frames != NULL) {
        for (uint i = 0; i < total_frames; i++) {
            unifex_payload_release(out_frames[i]);
            unifex_free(out_frames[i]);
        }
        unifex_free(out_frames);
        unifex_free(out_pts);
    }

    return res;
}

void handle_destroy_state(UnifexEnv* env, State* state) {
    if (state->dec != NULL) delete state->dec;

    UNIFEX_UNUSED(env);
    UNIFEX_UNUSED(state);
}
