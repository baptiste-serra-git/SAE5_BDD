/*
 * aht2x.c
 *
 *  Created on: Oct 11, 2023
 *      Author: Jean-Roch
 */

#include "aht2x.h"
#include "debugTerminal.h"

#define AHT21_ADD_7Bits 0x38
#define AHT21_ADD_8Bits AHT21_ADD_7Bits<<1

#define AHT21_busy_status_mask 0x80
#define AHT21_REGISTER 0xAC

//See https://github.com/enjoyneering/AHTxx

/**
 * Initialize AHT2x Device
 * @param device Address of a Aht2x_t variable
 * @param hi2c address of the STM32 i2c module Handle
 * @return
 *      - 0 if OK
 *      - -1 if an error occurs
 * @details This function should initialize the device structure & test if a Aht2x device is connected to the i2c Bus
 */
int aht2xInit(Aht2x_t *device, I2C_HandleTypeDef *hi2c)
{
	uint8_t status;
	device->hi2c = hi2c;
	if (aht2xReadStatus(device, &status) == -1)
	{
		debugLogErrorTrace("erreur initaht");
		return -1;
	}
	debugLogInfoTrace("status = %02X", status);
	return 0;
}


/**
 * Read AHT2x Device Status Byte
 * @param device Address of a Aht2x_t variable
 * @param status address of a variable where to store status byte value
 * @return
 *      - 0 if OK
 *      - -1 if an error occurs
 * @note This function should be base on HAL_I2C_Master_Receive()
 */
int aht2xReadStatus(Aht2x_t *device, uint8_t *status)
{
	/*if(!device->hi2c)
	 return -1;*/
	if (HAL_I2C_Master_Receive(device->hi2c, AHT21_ADD_8Bits, status, 1, 10)
			!= HAL_OK)
	{
		debugLogErrorTrace("master error");
		return -1;
	}
	return 0;

}

/**
 * Start AHT2x Device measurement (non blocking function)
 * @param device Address of a Aht2x_t variable
 * @return
 *     - 0 if OK
 *     - -1 if an error occurs
 * @note This function should be base on HAL_I2C_Mem_Write()
 */
int aht2xStartMeasure(Aht2x_t *device)
{
	uint8_t buffer[] = {0x30, 0};
	uint32_t status;
	status = HAL_I2C_Mem_Write(device->hi2c, AHT21_ADD_8Bits, AHT21_REGISTER, 1, buffer, 2, 10);
	if(status != HAL_OK){
		return -1;
	}
	if(!aht2xIsBusy(device)){
		return -1;
	}
	return 0;

}

/**
 * Check if AHT2x Device is busy
 * @param device Address of a Aht2x_t variable
 * @return
 *     - true if the device is ready
 *     - false if a measure is in progress
 * @note This function should be base on aht2xReadStatus()
 */
bool aht2xIsBusy(Aht2x_t *device)
{
	uint8_t status;
	if(aht2xReadStatus(device, &status)){
		debugLogErrorTrace("Busy");
		return false;
	}
	if((status & AHT21_busy_status_mask) != AHT21_busy_status_mask){
		return false;
		debugLogErrorTrace("Not Same Mask");
	}
	return true;
	debugLogInfoTrace("Not Busy");

}

/**
 * Start AHT2x Device measurement and wait for the end of measure (blocking function)
 * @param device Address of a Aht2x_t variable
 * @param timeout timeout in ms
 * @return
 *     - 0 if OK
 *     - -1 if an error occurs
 */
int aht2xStartAndPolMeasure(Aht2x_t *device, uint32_t timeout)
{
	uint32_t start_timer = HAL_GetTick();
	if(aht2xStartMeasure(device)){
		return -1;
	}
	while(aht2xIsBusy(device)){
		if(HAL_GetTick() > start_timer + timeout){
			return -1;
		}
	}
	if(aht2xReadRawData(device))
		return -1;
	aht2xProcessRawData(device);
	return 0;
}

/**
 * Read the AHT2x device raw data
 * @param device Address of a Aht2x_t variable
 * @return
 *     - 0 if OK
 *     - -1 if an error occurs
 * @note This function should be base on HAL_I2C_Master_Receive()
 */
int aht2xReadRawData(Aht2x_t *device)
{
	uint32_t halstatus;
	halstatus = HAL_I2C_Master_Receive(device->hi2c, AHT21_ADD_8Bits, device->rawData, sizeof(device->rawData), 1);
	if(halstatus != HAL_OK){
		return -1;
	}
	//debugTerminalLogArray(DEBUG_LEVEL_INFO, device->rawData, 7, "%02X ", "raw data = ");
	return 0;

}

/**
 * Process the Temperature and humidity values from the raw data
 * @param device Address of a Aht2x_t variable
 * @note The processed values of temperature and humidity should be stored in the device structure
 */
void aht2xProcessRawData(Aht2x_t *device)
{
	uint32_t temp, hum;
	hum = ((uint32_t)device->rawData[1] << 16) | ((uint32_t)device->rawData[2] << 8) | ((uint32_t)device->rawData[3]);
	hum = hum >> 4;

	temp = ((uint32_t)device->rawData[3] << 16) | ((uint32_t)device->rawData[4] << 8) | ((uint32_t)device->rawData[5]);
	temp &= 0x0FFFFF;
	//debugLogInfoTrace("hum = %d   temp = %d", hum, temp);

	device->temperature = (( (float)temp / (float)(1 << 20))*200-50)*100;
	device->humidity = (((float)hum / (float)(1 << 20))*100)*100;

	//debugLogInfoTrace("taux d'humidite = %f   temperature = %f", device->humidity, device->temperature);

}

/**
 * Get the real temperature value in °C
 * @param device Address of a Aht2x_t variable
 * @param forceUpdate set to true to force a full reading of the device, false otherwise
 * @param timeout timeout value in ms
 * @return Temperature value in °C
 */
float aht2xGetTemperature(Aht2x_t *device, bool forceUpdate, uint32_t timeout)
{
	if(!forceUpdate){
		aht2xProcessRawData(device);
		return device->temperature;
		//debugLogInfoTrace("temperature = %f", device->temperature);
	}
	if(aht2xStartAndPolMeasure(device, timeout))
		return 0;
	return device->temperature;
	//debugLogInfoTrace("temperature = %f", device->temperature);
}

/**
 * Get the real humidity value in %
 * @param device Address of a Aht2x_t variable
 * @param forceUpdate set to true to force a full reading of the device, false otherwise
 * @param timeout timeout value in ms
 * @return Humidity value in %
 */
float aht2xGetHumidity(Aht2x_t *device, bool forceUpdate, uint32_t timeout)
{
	if(!forceUpdate){
		aht2xProcessRawData(device);
		return device->humidity;
		debugLogInfoTrace("taux d'humidite = %f", device->humidity);
	}
	if(aht2xStartAndPolMeasure(device, timeout))
		return 0;
	return device->humidity;
	debugLogInfoTrace("taux d'humidite = %f", device->humidity);
}
