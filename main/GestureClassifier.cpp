#include <stdio.h>
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "GestureClassifier.h"

float *GestureClassifier::m_bufferPtr;

int GestureClassifier::readDataCB(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, GestureClassifier::m_bufferPtr + offset, length * sizeof(float));
  return 0;
}

void print_inference_result(ei_impulse_result_t result) {

    // Print how long it took to perform inference
    ei_printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
            result.timing.dsp,
            result.timing.classification,
            result.timing.anomaly);

    // Print the prediction results (object detection)
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    ei_printf("Object detection bounding boxes:\r\n");
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);
    }

    // Print the prediction results (classification)
#else
    ei_printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
        ei_printf("%.5f", result.classification[i].value);
        if (result.classification[i].value > 0.6) {
            printf("  <--");
        }
        printf("\n");
    }
#endif

    // Print anomaly result (if it exists)
#if EI_CLASSIFIER_HAS_ANOMALY == 1
    ei_printf("Anomaly prediction: %.3f\r\n", result.anomaly);
#endif

}

GestureClassifier::GestureClassifier(float confidenceTarget, gc_result_cb_t cb, void *ctx)
{
    m_confidenceTarget = confidenceTarget;
    m_resultCB = cb;
    m_resultCBCtx = ctx;
    m_bufferPtr = NULL;
    m_curBufferIdx = 0;
}

float *GestureClassifier::getBufferPtr()
{
    return m_bufferPtr;
}

int GestureClassifier::appendData(float ax, float ay, float az)
{
    if(m_curBufferIdx >= m_nSampleCount) return -1;
    m_bufferPtr[m_curBufferIdx++] = ax;
    m_bufferPtr[m_curBufferIdx++] = ay;
    m_bufferPtr[m_curBufferIdx++] = az;
    return 0;
}

void GestureClassifier::reset()
{
    m_curBufferIdx = 0;
    memset(m_bufferPtr, 0, m_nSampleCount* sizeof(float));
}

int GestureClassifier::setSampleCount(int nCount)
{
    if(m_bufferPtr!=NULL) free(m_bufferPtr);

    m_bufferPtr = (float *)malloc(nCount * sizeof(float));
    if(m_bufferPtr==NULL) return -1;
    memset(m_bufferPtr, 0, nCount* sizeof(float));
    m_curBufferIdx = 0;
    m_nSampleCount = nCount;
    return 0;
}

int GestureClassifier::run()
{
    ei_impulse_result_t result = { nullptr };

    signal_t features_signal;
    features_signal.total_length = m_curBufferIdx;
    features_signal.get_data = &GestureClassifier::readDataCB;

    // invoke the impulse
    EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false /* debug */);
    if (res != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", res);
        return res;
    }

    print_inference_result(result);
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > m_confidenceTarget) {
            if(m_resultCB) m_resultCB(result.classification[i].label, m_resultCBCtx);
            return 0;
        }
    }
    if(m_resultCB) m_resultCB(NULL, m_resultCBCtx);
    return 0;
}