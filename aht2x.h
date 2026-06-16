/*
 * aht2x.h
 *
 *  Created on: Oct 11, 2023
 *      Author: Jean-Roch
 */

#ifndef AHT2X_H_
#define AHT2X_H_

#include "main.h"
#include <stdbool.h>


typedef struct{
	I2C_HandleTypeDef* hi2c;
	uint8_t rawData[7];
	float temperature;
	float humidity;
}Aht2x_t;

int aht2xInit(Aht2x_t *device, I2C_HandleTypeDef *hi2c);
int aht2xReadStatus(Aht2x_t *device, uint8_t *status);
int aht2xStartMeasure(Aht2x_t *device);
int aht2xStartAndPolMeasure(Aht2x_t *device, uint32_t timeout);
bool aht2xIsBusy(Aht2x_t *device);
int aht2xReadRawData(Aht2x_t* device);
void aht2xProcessRawData(Aht2x_t *device);
float aht2xGetTemperature(Aht2x_t *device, bool forceUpdate, uint32_t timeout);
float aht2xGetHumidity(Aht2x_t *device, bool forceUpdate, uint32_t timeout);


#endif /* AHT2X_H_ */
