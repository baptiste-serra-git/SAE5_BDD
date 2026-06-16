/**
 * @file userAppTasks.h
 *
 * @date 26 oct. 2021
 * @author Jean-Roch
 */

#ifndef USERAPPTASKS_H_
#define USERAPPTASKS_H_

#include <stdint.h>

void userAppTaskInit(void);
void userAppTask(void);
void remplissage(void);

struct IV {
	uint32_t timestamp;
	uint32_t NoneID1;
	uint32_t timestamp2;
	uint32_t NoneID12;
};


struct TR {
	uint32_t timestamp;
	uint32_t NoneID1;

	uint32_t FrameCounter;
	uint16_t NoneID2;
	uint8_t len;
	uint8_t FrameType;

	uint16_t temp;
	uint16_t hum;
	uint8_t VBAT;
	uint8_t QAIR;
	uint16_t C02;
};

#endif /* USERAPPTASKS_H_ */
