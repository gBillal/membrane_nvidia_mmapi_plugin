#include "decoder.h"

using namespace std;

void dmabufToPayload(int dmabuf_fd, uint total_planes, UnifexPayload* payload)
{
    uint offset = 0;

    for (uint plane = 0; plane < total_planes; plane++) {
        NvBufSurface *nvbuf_surf = 0;
        if (NvBufSurfaceFromFd(dmabuf_fd, (void**)(&nvbuf_surf)) < 0) {
            throw std::runtime_error("could not create buf surface");
        }

        if (NvBufSurfaceMap(nvbuf_surf, 0, plane, NVBUF_MAP_READ_WRITE) < 0) {
            throw std::runtime_error("could not map buf surface");
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

        if (NvBufSurfaceUnMap(nvbuf_surf, 0, plane) < 0) {
            throw std::runtime_error("could not unmap buf surface");
        }
    }
}

pair<vector<UnifexPayload*>, vector<int64_t>> getDecodedFrames(UnifexEnv* env, State* state)
{
    vector<UnifexPayload*> frames;
    vector<int64_t> pts;
    
    while(auto pair = state->dec->nextFrame())
    {
        UnifexPayload* payload = (UnifexPayload*)unifex_alloc(sizeof(UnifexPayload));
        unifex_payload_alloc(env, UNIFEX_PAYLOAD_BINARY, state->dec->frameSize(), payload);
        dmabufToPayload(get<int>(*pair), 3, payload);
        
        frames.push_back(payload);
        pts.push_back(get<int64_t>(*pair));
    }

    return std::make_pair(frames, pts);
}

UNIFEX_TERM create(UnifexEnv *env, char* pix_fmt, int width, int height) {
    UNIFEX_TERM res;
    State *state = unifex_alloc_state(env);
    
    try {
        state->dec = Decoder::createDecoder(pix_fmt, width, height);
        res = create_result_ok(env, state);
    } catch (exception& e) {
        res = create_result_error(env, e.what());
    }

    // unifex_release_state(env, state);
    return res;
}

UNIFEX_TERM decode(UnifexEnv *env, UnifexPayload* payload, int64_t timestamp, State* state) {
    UNIFEX_TERM res;

    try {
        state->dec->process(payload->data, payload->size, timestamp);
        auto frames_and_pts = getDecodedFrames(env, state);
        auto frames = frames_and_pts.first;
        auto pts = frames_and_pts.second;

        res = decode_result_ok(env, frames.data(), frames.size(), pts.data(), pts.size());

        for (size_t i = 0; i < frames.size(); i++) {
            unifex_payload_release(frames[i]);
            unifex_free(frames[i]);
        }
    } catch (exception& e) {
        res = decode_result_error(env, e.what());
    }
  
    return res;
}

UNIFEX_TERM flush(UnifexEnv* env, State* state) {
    UNIFEX_TERM res;

    try {
        state->dec->flush();
        auto frames_and_pts = getDecodedFrames(env, state);
        auto frames = frames_and_pts.first;
        auto pts = frames_and_pts.second;

        res = decode_result_ok(env, frames.data(), frames.size(), pts.data(), pts.size());
    
        for (size_t i = 0; i < frames.size(); i++) {
            unifex_payload_release(frames[i]);
            unifex_free(frames[i]);
        }
    } catch (exception& e) {
        res = decode_result_error(env, e.what());
    }

    return res;
}

void handle_destroy_state(UnifexEnv* env, State* state) {
    if (state->dec != NULL) delete state->dec;

    UNIFEX_UNUSED(env);
    UNIFEX_UNUSED(state);
}
