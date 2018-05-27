#pragma once

#include <I2C.h>
#include <SPI.h>

enum BME280_RESULT {
    BME280_OK = 0
};

class BME280Config {
public:
    enum T_SB{
	T_SB_0_5ms = 0,
	T_SB_62_5ms = 1,
	T_SB_125ms = 2,
	T_SB_250ms = 3,
	T_SB_500ms = 4,
	T_SB_1000ms = 5,
	T_SB_10sec = 6,
	T_SB_20sec = 7
    };
    enum FILTER{
	FILTER_OFF = 0,
	FILTER_2 = 1,
	FILTER_4 = 2,
	FILTER_8 = 3,
	FILTER_16 = 4
    };
    enum OS_MODE {
	OS_SKIP = 0,
	OS_x1 = 1,
	OS_x2 = 2,
	OS_x4 = 3,
	OS_x8 = 4,
	OS_x16 = 5
    };

    T_SB    t_sb;
    FILTER  filter;
    bool    enable3WireSPI;
    OS_MODE osrsTemperature;
    OS_MODE osrsHumidity;
    OS_MODE osrsPressure;

    BME280Config() :
	t_sb(T_SB_1000ms),
	filter(FILTER_OFF),
	enable3WireSPI(false),
	osrsTemperature(OS_x1),
	osrsHumidity(OS_x1),
	osrsPressure(OS_x1) {};
};

class BME280 {
protected:
    struct {
	uint16_t dig_T1;
	int16_t  dig_T2;
	int16_t  dig_T3;

	uint16_t dig_P1;
	int16_t  dig_P2;
	int16_t  dig_P3;
	int16_t  dig_P4;
	int16_t  dig_P5;
	int16_t  dig_P6;
	int16_t  dig_P7;
	int16_t  dig_P8;
	int16_t  dig_P9;

	uint8_t  dig_H1;
	int16_t  dig_H2;
	uint8_t  dig_H3;
	int16_t  dig_H4;
	int16_t  dig_H5;
	int8_t   dig_H6;
    }cdata;
    BME280Config config;

    int32_t      tempRaw;
    int32_t      humRaw;
    int32_t      presRaw;

    int32_t      tempFine;

    int32_t      temperature;
    uint32_t     humidity;
    uint32_t     pressure;
    
public:
    BME280();
    virtual ~BME280();

    void init(BME280Config* config = NULL);
    void start(bool asForcedMode = false);
    void stop();

    void measure();

    inline int32_t getTemperature(){return temperature;};
    inline float getTemperatureFloat(){return (float)temperature / 100.;};
    inline uint32_t getHumidity(){return humidity;};
    inline float getHumidityFloat(){return (float)humidity / 1024.;};
    inline uint32_t getPressure(){return pressure;};
    inline float getPressureFloat(){return (float)pressure / 25600.;};

protected:
    virtual uint8_t readRegister(uint8_t addr) = 0;
    virtual void readRegisters(uint8_t addr, uint8_t* buf, size_t len) = 0;
    virtual void writeRegister(uint8_t addr, uint8_t data) = 0;
    virtual void writeRegisters(uint8_t addr, uint8_t* data, size_t len) = 0;

    inline uint16_t readUint16(uint8_t laddr, uint8_t haddr){
	return
	    (uint16_t)readRegister(laddr) |
	    ((((uint16_t)readRegister(haddr)) << 8) & 0xff00);
    };
    inline int16_t readInt16(uint8_t laddr, uint8_t haddr){
	return (int16_t)readUint16(laddr, haddr);
    };

    void compensateTemperature();
    void compensateHumidity();
    void compensatePressure();
};

class BME280_I2C : public BME280{
protected:
    I2C i2c;

public:
    BME280_I2C(uint8_t address,
	       gpio_num_t sdaPin = I2C::DEFAULT_SDA_PIN,
	       gpio_num_t sclPin = I2C::DEFAULT_CLK_PIN,
	       uint32_t clkSpeed = I2C::DEFAULT_CLK_SPEED,
	       i2c_port_t portNum = I2C_NUM_0,
	       bool builtinPullup = true);
    virtual ~BME280_I2C();

protected:
    virtual uint8_t readRegister(uint8_t addr);
    virtual void readRegisters(uint8_t addr, uint8_t* buf, size_t len);
    virtual void writeRegister(uint8_t addr, uint8_t data);
    virtual void writeRegisters(uint8_t addr, uint8_t* data, size_t len);
};
