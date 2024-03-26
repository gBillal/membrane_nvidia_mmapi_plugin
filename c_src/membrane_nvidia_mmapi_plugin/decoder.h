#pragma once

#include "NvVideoDecoder.h"
#include "NvEglRenderer.h"
#include <fstream>

#include "NvBufSurface.h"
#include "NvUtils.h"

#define MICROSECOND_UNIT 1000000
#define MAX_BUFFERS 10
#define CHUNK_SIZE 4000000

typedef struct _decoder_state {
    NvVideoDecoder *dec;

    int width;
    int height;

    bool wait_for_resolution_event;
    bool got_error;
    bool eos;
    
    int dst_dma_fd;
    int buf_idx;
} State;

#define ERR_DQ_EVENT -1
#define ERR_GET_FORMAT -2
#define ERR_GET_CROP -3
#define ERR_NV_ALLOCATE -4
#define ERR_GET_MIN_CAPTURE_PLANES -5
#define ERR_SET_CAPTURE_PLANE_FORMAT -6
#define ERR_SETUP_PLANE -7
#define ERR_SET_STREAM_STATUS -8
#define ERR_CAPTURE_PLANE_QBuffer -9
#define ERR_CAPTURE_PLANE_DQ_Buffer -10
#define ERR_NV_TRANSFORM -11

#include "_generated/decoder.h"