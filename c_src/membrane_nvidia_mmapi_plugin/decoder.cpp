#include "decoder.h"
#include <stdexcept>

Decoder* Decoder::createDecoder(const char* pix_fmt, int width, int height) 
{
    NvVideoDecoder *dec = NvVideoDecoder::createVideoDecoder("dec0", O_NONBLOCK);
    if (!dec) throw std::runtime_error("Failed to create NvVideoDecoder");

    if(dec->subscribeEvent(V4L2_EVENT_RESOLUTION_CHANGE, 0, 0) < 0)
    {
        delete dec;
        throw std::runtime_error("Failed to subscribe to event resolution change");
    }

    int output_plane_pix_fmt;
    if (strcmp(pix_fmt, "H264") == 0) output_plane_pix_fmt = V4L2_PIX_FMT_H264;
    else output_plane_pix_fmt = V4L2_PIX_FMT_H265;

    if(dec->setOutputPlaneFormat(output_plane_pix_fmt, MaxFrameSize) < 0)
    {
        delete dec;
        throw std::runtime_error("Failed to set output plane format");
    }

    if(dec->setFrameInputMode(0) < 0)
    {
        delete dec;
        throw std::runtime_error("Failed to set frame input mode");
    }

    // ret = state->dec->setSkipFrames(V4L2_SKIP_FRAMES_TYPE_DECODE_IDR_ONLY);
    // TEST_ERROR(ret < 0, create_result_error, env, "skip_frames");

    if(dec->output_plane.setupPlane(V4L2_MEMORY_USERPTR, MaxBuffers, false, true) < 0)
    {
        delete dec;
        throw std::runtime_error("Failed to setup output plane");
    }

    if(dec->output_plane.setStreamStatus(true) < 0)
    {
        delete dec;
        throw std::runtime_error("Failed to set stream status of output plane");
    }

    Decoder* decoder = new Decoder();
    decoder->m_dec = dec;
    decoder->m_width = width;
    decoder->m_height = height;
    
    return decoder;
}

int Decoder::frameSize() {return m_width * m_height * 3 / 2;}

void Decoder::process(unsigned char* data, int size, int64_t pts)
{
    this->qBuffer(data, size, pts);
    if (this->m_waitingForResolutionEvent) this->setCapturePlane();
}

void Decoder::flush()
{
    this->m_eos = true;
    this->qBuffer(NULL, 0, 0);
}

optional<pair<int, int64_t>> Decoder::nextFrame()
{
    bool full_output_plane = this->m_dec->output_plane.getNumQueuedBuffers() == MaxBuffers;

    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];
    NvBuffer* buffer = NULL;

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, sizeof(planes));
    v4l2_buf.m.planes = planes;

    while(this->m_dec->capture_plane.dqBuffer(v4l2_buf, &buffer, NULL, 0))
    {
        if (errno == EAGAIN) {
            // If it's the end of stream, we'll wait for all the frames to be decoded
            // or if all the buffers are queued, we'll wait for the decoder
            // to decode at least one frame
            int last_buf = v4l2_buf.flags & V4L2_BUF_FLAG_LAST;
            if (this->m_eos && !last_buf) continue;
            else if (full_output_plane) continue;
            else return nullopt;
        } 
        
        throw std::runtime_error("could not dequeue buffer from capture plane");
    }

    if (dqBuffer() < 0) 
    {
        throw std::runtime_error("could not dequeue buffer from output plane");
    }

    NvBufSurf::NvCommonTransformParams transform_params;
    transform_params.flag = NVBUFSURF_TRANSFORM_FILTER;
    transform_params.flip = NvBufSurfTransform_None;
    transform_params.filter = NvBufSurfTransformInter_Nearest;
    
    if (NvBufSurf::NvTransform(&transform_params, buffer->planes[0].fd, this->m_dstDmaFd) < 0)
    {
        throw std::runtime_error("could not transform DMA buffer");
    }

    if (this->m_dec->capture_plane.qBuffer(v4l2_buf, NULL) < 0)
    {
        throw std::runtime_error("could not queue buffer to capture plane");
    }

    int64_t pts = v4l2_buf.timestamp.tv_sec * Microsecond + v4l2_buf.timestamp.tv_usec;
    return make_optional(make_pair(m_dstDmaFd, pts));
}

Decoder::~Decoder()
{
    while(dqBuffer() == 0);
    delete this->m_dec;
    if (m_dstDmaFd != -1) NvBufSurf::NvDestroy(m_dstDmaFd);
}

void Decoder::qBuffer(unsigned char* data, int size, int64_t pts) 
{
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];

    NvBuffer* buffer = this->m_dec->output_plane.getNthBuffer(this->m_bufIdx);
    if (data != NULL) {
        buffer->planes[0].data = data;
        buffer->planes[0].bytesused = size;
    } else {
        buffer->planes[0].bytesused = 0;
    }

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, sizeof(planes));

    v4l2_buf.index = this->m_bufIdx;
    v4l2_buf.m.planes = planes;
    v4l2_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
    
    if (data != NULL) {
        v4l2_buf.flags |= V4L2_BUF_FLAG_TIMESTAMP_COPY;
        v4l2_buf.timestamp.tv_sec = pts / Microsecond;
        v4l2_buf.timestamp.tv_usec = pts % Microsecond;
    }

    this->m_bufIdx = (this->m_bufIdx + 1) % MaxBuffers;

    if(this->m_dec->output_plane.qBuffer(v4l2_buf, NULL) < 0)
    {
        throw std::runtime_error("could not queue buffer to output plane");
    }
}

int Decoder::dqBuffer() {
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[MAX_PLANES];

    memset(&v4l2_buf, 0, sizeof(v4l2_buf));
    memset(planes, 0, sizeof(planes));

    v4l2_buf.m.planes = planes;

    return this->m_dec->output_plane.dqBuffer(v4l2_buf, NULL, NULL, -1);
}

void Decoder::setCapturePlane() 
{
    NvVideoDecoder* dec = this->m_dec;
    struct v4l2_event event;

    do {
        if (dec->dqEvent(event, 1000) < 0)
        {
            throw std::runtime_error("could not dequeue event");
        }
    } while (event.type != V4L2_EVENT_RESOLUTION_CHANGE);

    v4l2_format format;
    v4l2_crop crop;
    int32_t min_dec_capture_buffers;

    if (dec->capture_plane.getFormat(format) < 0) {
        throw std::runtime_error("could not get format from capture plane");
    }

    if (dec->capture_plane.getCrop(crop) < 0) {
        throw std::runtime_error("could not get crop from capture plane");
    }

    this->m_width = this->m_width == -1 ? crop.c.width : this->m_width;
    this->m_height = this->m_height == -1 ? crop.c.height : this->m_height;

    NvBufSurf::NvCommonAllocateParams params;
    params.memType = NVBUF_MEM_SURFACE_ARRAY;
    params.width = this->m_width;
    params.height = this->m_height;
    params.layout = NVBUF_LAYOUT_PITCH;
    params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;
    params.memtag = NvBufSurfaceTag_VIDEO_CONVERT;

    if (NvBufSurf::NvAllocate(&params, 1, &this->m_dstDmaFd) < 0) {
        throw std::runtime_error("could not allocate DMA buffer");
    }

    dec->capture_plane.deinitPlane();
    if (dec->getMinimumCapturePlaneBuffers(min_dec_capture_buffers) < 0) {
        throw std::runtime_error("could not get minimum capture plane buffers");
    }

    int ret = dec->setCapturePlaneFormat(format.fmt.pix_mp.pixelformat, 
                                    format.fmt.pix_mp.width, 
                                    format.fmt.pix_mp.height);
    if (ret < 0) {
        throw std::runtime_error("could not set capture plane format");
    }

    if (dec->capture_plane.setupPlane(V4L2_MEMORY_MMAP, min_dec_capture_buffers, false, false) < 0) {
        throw std::runtime_error("could not setup capture plane");
    }
    
    if (dec->capture_plane.setStreamStatus(true) < 0) {
        throw std::runtime_error("could not set stream status of capture plane");
    }

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
            throw std::runtime_error("could not queue buffer on capture plane");
        }
    }

    this->m_waitingForResolutionEvent = false;
}
