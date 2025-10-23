#pragma once
#include <stdio.h>

typedef void (*gc_result_cb_t)(const char *label, void *ctx);

class GestureClassifier {
private:
    static float *m_bufferPtr;
    int m_nSampleCount;
    int m_curBufferIdx;
    float m_confidenceTarget;
    gc_result_cb_t m_resultCB;
    void *m_resultCBCtx;
    static int readDataCB(size_t offset, size_t length, float *out_ptr);
public:
    GestureClassifier(float confidenceTarget = 0.65, gc_result_cb_t cb = NULL, void *ctx=NULL);
    float *getBufferPtr();
    int setSampleCount(int nCount);
    int appendData(float ax, float ay, float az);
    void reset();
    int run();
};

