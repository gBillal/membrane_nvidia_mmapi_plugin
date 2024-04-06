#pragma once

#include <vector>
#include "NvVideoDecoder.h"
#include "NvBufSurface.h"

using namespace std;

class Decoder
{
private:
    static const int Microsecond = 1000000;
    static const int MaxBuffers = 10;
    static const int MaxFrameSize = 4000000;

    NvVideoDecoder* m_dec;
    int m_width;
    int m_height;
    int m_dstDmaFd = -1;
    int m_bufIdx;
    bool m_waitingForResolutionEvent = true;
    bool m_eos = false;

    void qBuffer(unsigned char* data, int size, int64_t pts);
    int dqBuffer();
    void setCapturePlane();
public:
    static Decoder* createDecoder(const char* pix_fmt, int width, int height);
    ~Decoder();

    int frameSize();
    void process(unsigned char* data, int size, int64_t pts);
    optional<pair<int, int64_t>> nextFrame();
    void flush();
};

typedef struct _decoder_state {
    Decoder *dec;
} State;

#include "_generated/decoder.h"