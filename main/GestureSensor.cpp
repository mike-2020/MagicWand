#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "esp_log.h"
#include "driver/i2c.h"
//#include "MPU6050.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include "GestureSensor.h"
#include "PinDef.h"

void get_kalman_mpu_data(MPU6050 *mpu, float *Ox, float *Oy, float *Oz);
void resetMPUState(MPU6050 *mpu);

static const char *TAG = "GestureSensor";
MPU6050 g_mpu6050;
GestureSensor g_gestureSensor;

GestureSensor* GestureSensor::getInstance()
{
    return &g_gestureSensor;
}

int GestureSensor::prepareI2C()
{
	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = (gpio_num_t)GPIO_IDF_I2C_SDA;
	conf.scl_io_num = (gpio_num_t)GPIO_IDF_I2C_SCL;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = 400000;
	conf.clk_flags = 0;
	ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_1, &conf));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0));
    return ESP_OK;
}

int GestureSensor::init()
{
    uint8_t devStatus;
    prepareI2C();

    g_mpu6050.initialize();
	// Get Device ID
	uint8_t buffer[1];
	I2Cdev::readByte(MPU6050_ADDRESS_AD0_LOW, MPU6050_RA_WHO_AM_I, buffer);
	ESP_LOGI(TAG, "getDeviceID=0x%x", buffer[0]);

	// Initialize DMP
	devStatus = g_mpu6050.dmpInitialize();
	ESP_LOGI(TAG, "devStatus=%d", devStatus);
	if (devStatus != 0) {
		ESP_LOGE(TAG, "DMP Initialization failed [%d]", devStatus);
        return ESP_FAIL;
	}
    //g_mpu6050.setFullScaleAccelRange(MPU6050_ACCEL_FS_4);
    g_mpu6050.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
    g_mpu6050.setRate(100); //set to 100Hz

	// This need to be setup individually
	// supply your own gyro offsets here, scaled for min sensitivity
	//g_mpu6050.setXAccelOffset(30);
	//g_mpu6050.setYAccelOffset(10);
	//g_mpu6050.setZAccelOffset(0);
	//g_mpu6050.setXGyroOffset(149);
	//g_mpu6050.setYGyroOffset(27);
	//g_mpu6050.setZGyroOffset(17);
	g_mpu6050.setXGyroOffset(220);
	g_mpu6050.setYGyroOffset(76);
	g_mpu6050.setZGyroOffset(-85);
	g_mpu6050.setZAccelOffset(1788);

	// Calibration Time: generate offsets and calibrate our MPU6050
	g_mpu6050.CalibrateAccel(6);
	g_mpu6050.CalibrateGyro(6);
	g_mpu6050.setDMPEnabled(true);

    resetMPUState(&g_mpu6050);

    ESP_LOGI(TAG, "GestureSensor has been initialized.");
    return 0;
}

int GestureSensor::getAcceleration(int16_t *ax, int16_t *ay, int16_t *az)
{
    g_mpu6050.getAcceleration(ax, ay, az);
    float x, y, z;
    x = ((float)*ax / 8192);
    y = ((float)*ay / 8192);
    z = ((float)*az / 8192);
    printf("acc x=%.2f, y=%.2f, z=%.2f", x, y, z);
    return 0;
}

int GestureSensor::getAcceleration(float *ax, float *ay, float *az)
{
    int16_t x, y, z;
    g_mpu6050.getAcceleration(&x, &y, &z);

    *ax = ((float)x / 8192) * 9.8;
    *ay = ((float)y / 8192) * 9.8;
    *az = ((float)z / 8192) * 9.8;
    printf("acc x=%.3f, y=%.3f, z=%.3f\n", *ax, *ay, *az);
    return 0;
}

int GestureSensor::getRotation(int16_t *rx, int16_t *ry, int16_t *rz)
{
    g_mpu6050.getRotation(rx, ry, rz);
    ESP_LOGI(TAG, "R x=%d, y=%d, z=%d", *rx, *ry, *rz);
    return 0;
}


int GestureSensor::getRealAcceleration(int16_t *ax, int16_t *ay, int16_t *az)
{
    Quaternion q;			// [w, x, y, z]			quaternion container
    VectorInt16 aa;			// [x, y, z]			accel sensor measurements
    VectorInt16 aaReal;		// [x, y, z]			gravity-free accel sensor measurements
    VectorFloat gravity;	// [x, y, z]			gravity vector
    uint8_t fifoBuffer[64]; // FIFO storage buffer

    g_mpu6050.dmpGetCurrentFIFOPacket(fifoBuffer);
	g_mpu6050.dmpGetQuaternion(&q, fifoBuffer);
	g_mpu6050.dmpGetAccel(&aa, fifoBuffer);
	g_mpu6050.dmpGetGravity(&gravity, &q);
	g_mpu6050.dmpGetLinearAccel(&aaReal, &aa, &gravity);
    float x, y, z;
    x = ((float)aa.x / 8192);
    y = ((float)aa.y / 8192);
    z = ((float)aa.z / 8192);
    printf("acc x=%.2f, y=%.2f, z=%.2f | ", x, y, z);

    *ax = aaReal.x;
    *ay = aaReal.y;
    *az = aaReal.z;

    x = ((float)aaReal.x / 8192);
    y = ((float)aaReal.y / 8192);
    z = ((float)aaReal.z / 8192);
	printf("ACC x=%.2f, y=%.2f, z=%.2f\n", x, y, z);
    return 0;
}

int GestureSensor::getTransformedMotionData(float *Ox, float *Oy, float *Oz)
{
    get_kalman_mpu_data(&g_mpu6050, Ox, Oy, Oz);
    return 0;
}

static float vertZero = 0, horzZero = 0, scrlZero = 0;
int GestureSensor::getDataForMouse(float *vertValue, float *horzValue, float *scrlValue)
{
    // MPU vars
    Quaternion q;           // [w, x, y, z]         quaternion container
    VectorFloat gravity;    // [x, y, z]            gravity vector
    float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
    uint16_t packetSize = 42;    // expected DMP packet size (default is 42 bytes)
    uint16_t fifoCount;     // count of all bytes currently in FIFO
    uint8_t fifoBuffer[64]; // FIFO storage buffer
    uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
    float yaw = 0.0, pitch = 0.0, roll = 0.0;

    *vertValue = 0;
    *horzValue = 0;
    *scrlValue = 0;

    mpuIntStatus = g_mpu6050.getIntStatus();
    // get current FIFO count
    fifoCount = g_mpu6050.getFIFOCount();

    if ((mpuIntStatus & 0x10) || fifoCount == 1024)
    {
        // reset so we can continue cleanly
        g_mpu6050.resetFIFO();
        ESP_LOGW(TAG, "Reset MPU FIFO.");
        return -1;
        // otherwise, check for DMP data ready interrupt frequently)
    }
    else if (mpuIntStatus & 0x02)
    {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize)
            fifoCount = g_mpu6050.getFIFOCount();

        // read a packet from FIFO

        g_mpu6050.getFIFOBytes(fifoBuffer, packetSize);
        g_mpu6050.dmpGetQuaternion(&q, fifoBuffer);
        g_mpu6050.dmpGetGravity(&gravity, &q);
        g_mpu6050.dmpGetYawPitchRoll(ypr, &q, &gravity);
        yaw = ypr[2] /M_PI * 180;
        pitch = ypr[1] /M_PI * 180;
        roll = ypr[0] /M_PI * 180;
        *vertValue = yaw - vertZero;
        *horzValue = roll - horzZero;
        *scrlValue = pitch - scrlZero;
        vertZero = yaw;
        horzZero = roll;
        scrlZero = pitch;
    }
    return 0;
}

int GestureSensor::getDataForMouse(int16_t *vertValue, int16_t *horzValue, int16_t *scrlValue)
{
    int16_t gx = 0, gy = 0, gz = 0;
    g_mpu6050.getRotation(&gx, &gy, &gz);
    *horzValue = gz/256;
    *vertValue = gy/256;
    return 0;
}