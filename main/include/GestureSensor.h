#pragma once
#include <stdint.h>

class GestureSensor{
private:
    void *m_handle;
    int prepareI2C();
public:
    int init();
    int getRealAcceleration(int16_t *x, int16_t *y, int16_t *z);
    int getAcceleration(int16_t *x, int16_t *y, int16_t *z);
    int getAcceleration(float *ax, float *ay, float *az);
    int getRotation(int16_t* x, int16_t* y, int16_t* z);
    int getTransformedMotionData(float *Ox, float *Oy, float *Oz);
    int getDataForMouse(float *vertValue, float *horzValue, float *scrlValue);
    int getDataForMouse(int16_t *vertValue, int16_t *horzValue, int16_t *scrlValue);
    static GestureSensor* getInstance();
};


typedef struct {
    int16_t ax;
    int16_t ay;
    int16_t az;
}gesture_data_point_t;


