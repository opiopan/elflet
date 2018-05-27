#include <esp_log.h>
#include <string.h>
#include "bme280.h"

static const char tag[] = "bme280";

static const uint8_t REG_ID = 0xd0;
static const uint8_t REG_RESET = 0xe0;
static const uint8_t REG_CTRL_HUM = 0xf2;
static const uint8_t REG_STATUS = 0xf3;
static const uint8_t REG_CTRL_MEAS = 0xf4;
static const uint8_t REG_CONFIG = 0xf5;

enum MEASURE_MODE {
    MODE_SLEEP = 0,
    MODE_FORCED = 1,
    MODE_NORMAL = 3
};

//----------------------------------------------------------------------
// BME280 constructor / destructor
//----------------------------------------------------------------------
BME280::BME280() {
    memset(&cdata, 0, sizeof(cdata));
}

BME280::~BME280() {
}

//----------------------------------------------------------------------
// Initialize device
//----------------------------------------------------------------------
void BME280::init(BME280Config* newConfig) {
    // set several configuration registers
    if (newConfig){
	config = *newConfig;
    }
    stop();
    writeRegister(
	REG_CONFIG,
	(config.t_sb << 5) | (config.filter << 2) | config.enable3WireSPI);
    writeRegister(REG_CTRL_HUM, config.osrsHumidity);

    printf("BME280: ctrl_meas = 0x%.2x\n", readRegister(REG_CTRL_MEAS));
    
    // retrieve compensation parameters
    cdata.dig_T1 = readUint16(0x88, 0x89);
    cdata.dig_T2 = readInt16(0x8a, 0x8b);
    cdata.dig_T3 = readInt16(0x8c, 0x8d);

    cdata.dig_P1 = readUint16(0x8e, 0x8f);
    cdata.dig_P2 = readInt16(0x90, 0x91);
    cdata.dig_P3 = readInt16(0x92, 0x93);
    cdata.dig_P4 = readInt16(0x94, 0x95);
    cdata.dig_P5 = readInt16(0x96, 0x97);
    cdata.dig_P6 = readInt16(0x98, 0x99);
    cdata.dig_P7 = readInt16(0x9a, 0x9b);
    cdata.dig_P8 = readInt16(0x9c, 0x9d);
    cdata.dig_P9 = readInt16(0x9e, 0x9f);

    cdata.dig_H1 = readRegister(0xa1);
    cdata.dig_H2 = readInt16(0xe1, 0xe2);
    cdata.dig_H3 = readRegister(0xe3);
    cdata.dig_H4 = (int16_t)(
	(((uint16_t)readRegister(0xe4) << 4) & 0xff0) |
	(readRegister(0xe5) & 0xf));
    cdata.dig_H5 = (int16_t)(
	(((uint16_t)readRegister(0xe6) << 4) & 0xff0) |
	((readRegister(0xe5) >> 4) & 0xf));
    cdata.dig_H6 = (int8_t)readRegister(0xe7);
}

//----------------------------------------------------------------------
// start & stop
//----------------------------------------------------------------------
void BME280::start(bool asForcedMode){
    uint8_t mode = asForcedMode ? MODE_FORCED : MODE_NORMAL;
    writeRegister(
	REG_CTRL_MEAS,
	(config.osrsTemperature << 5) | (config.osrsPressure << 2) | mode);
}

void BME280::stop(){
    writeRegister(
	REG_CTRL_MEAS,
	(config.osrsTemperature << 5) | (config.osrsPressure << 2) | 
	MODE_SLEEP);
}


//----------------------------------------------------------------------
// measure
//----------------------------------------------------------------------
void BME280::measure() {
    uint8_t data[8];
    readRegisters(0xf7, data, sizeof(data));

    presRaw = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    tempRaw = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    humRaw = (data[6] << 8) | data[7];

    compensateTemperature();
    compensatePressure();
    compensateHumidity();
}

void BME280::compensateTemperature(){
    int32_t var1, var2;
    var1 = ((((tempRaw >> 3) - ((int32_t)cdata.dig_T1 << 1))) *
	    ((int32_t)cdata.dig_T2)) >> 11;
    var2 = (((((tempRaw >> 4) - ((int32_t)cdata.dig_T1)) *
	      ((tempRaw >> 4) - ((int32_t)cdata.dig_T1))) >> 12) *
	    ((int32_t)cdata.dig_T3)) >> 14;
    tempFine = var1 + var2;
    temperature = (tempFine * 5 + 128) >> 8;
}

void BME280::compensatePressure(){
    int64_t var1, var2, p;
    var1 = ((int64_t)tempFine) - 128000;
    var2 = var1 * var1 * (int64_t)cdata.dig_P6;
    var2 = var2 + ((var1 * (int64_t)cdata.dig_P5) << 17);
    var2 = var2 + (((int64_t)cdata.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)cdata.dig_P3) >> 8) +
	((var1 * (int64_t)cdata.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)cdata.dig_P1) >> 33;
    if (var1 == 0){
	pressure = 0; // avoid exception caused by division by zero
	return; 
    }
    p = 1048576 - presRaw;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)cdata.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)cdata.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)cdata.dig_P7) << 4);
    pressure =  (uint32_t)p;
}

void BME280::compensateHumidity(){
    int32_t v_x1_u32r;
    v_x1_u32r = (tempFine - ((int32_t)76800));
    v_x1_u32r = (((((humRaw << 14) - (((int32_t)cdata.dig_H4) << 20) -
		    (((int32_t)cdata.dig_H5) * v_x1_u32r)) +
		   ((int32_t)16384)) >> 15) *
		 (((((((v_x1_u32r * ((int32_t)cdata.dig_H6)) >> 10) *
		      (((v_x1_u32r * ((int32_t)cdata.dig_H3)) >> 11) +
		       ((int32_t)32768))) >> 10) + ((int32_t)2097152)) *
		   ((int32_t)cdata.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) *
				 (v_x1_u32r >> 15)) >> 7) *
			       ((int32_t)cdata.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    humidity = (uint32_t)(v_x1_u32r >> 12);
}

//----------------------------------------------------------------------
// I2C behavior
//----------------------------------------------------------------------
BME280_I2C::BME280_I2C(uint8_t address,
		       gpio_num_t sdaPin,
		       gpio_num_t sclPin,
		       uint32_t clkSpeed,
		       i2c_port_t portNum,
		       bool builtinPullup){
    i2c.init(address, sdaPin, sclPin, clkSpeed, portNum, builtinPullup);
    i2c.scan();
}

BME280_I2C::~BME280_I2C() {
}

uint8_t BME280_I2C::readRegister(uint8_t addr){
    i2c.beginTransaction();
    i2c.write(addr);
    i2c.endTransaction();

    uint8_t data;
    i2c.beginTransaction();
    i2c.read(&data, false);
    i2c.endTransaction();
    return data;
}

void BME280_I2C::readRegisters(uint8_t addr, uint8_t* buf, size_t len){
    i2c.beginTransaction();
    i2c.write(addr);
    i2c.endTransaction();

    i2c.beginTransaction();
    i2c.read(buf, len -1);
    i2c.read(buf + len - 1, 1, false);
    i2c.endTransaction();
}

void BME280_I2C::writeRegister(uint8_t addr, uint8_t data){
    i2c.beginTransaction();
    i2c.write(addr);
    i2c.write(data);
    i2c.endTransaction();
}

void BME280_I2C::writeRegisters(uint8_t addr, uint8_t* data, size_t len){
    i2c.beginTransaction();
    i2c.write(addr);
    i2c.write(data, len);
    i2c.endTransaction();
}
