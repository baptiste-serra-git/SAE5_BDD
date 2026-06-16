/**
 * @file userAppTasks.c
 *
 * @date 26 oct. 2021
 * @author Jean-Roch
 */

#include "userAppTasks.h"
#include "debugTerminal.h"
//#include "tickImpoved.h"
#include "nrf24l01.h"
#include "spi.h"
#include "i2cEnhanced.h"
#include "i2c.h"
#include "aht2x.h"
#include "adc.h"
#include "tinyAES.h"


typedef enum
{
	USER_APP_TASK_STATE_INIT = 0, USER_APP_TASK_STATE_IDLE,
} UserAppTaskState_t;

static const uint8_t rxAdd[5] =
{ 1, 2, 3, 4, 5 };
static const uint8_t txAdd[5] =
{ 1, 2, 3, 4, 6 };

static uint8_t nrfRxBuffer[24] =
{ 0 };

struct TR trame = {0};
struct IV InitVector = {0};
uint32_t count = 0;
uint8_t key[] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};

struct AES_ctx ctx;

nrf24l01 nrf =
{ 0 };
nrf24l01_config nrfConfig =
{
		.addr_width = NRF_ADDR_WIDTH_5,
		.ce_pin = NRF_CE_Pin,
		.ce_port = NRF_CE_GPIO_Port,
		.crc_width = NRF_CRC_WIDTH_2B,
		.csn_pin = NRF_CSN_Pin,
		.csn_port = NRF_CSN_GPIO_Port,
		.data_rate = NRF_DATA_RATE_250KBPS,
		.irq_pin = NRF_INT_Pin,
		.irq_port = NRF_INT_GPIO_Port,
		.payload_length = sizeof(nrfRxBuffer),
		.retransmit_count = 15,
		.retransmit_delay = 0x0F, // 4000us, LSB:250us
		.rf_channel = 0,
		.rx_address = rxAdd,
		.rx_buffer = nrfRxBuffer,
		.spi = &hspi1,
		.spi_timeout = 10,
		.tx_address = txAdd,
		.tx_power = NRF_TX_PWR_M18dBm, };

Aht2x_t aht21;

void userAppTaskInit(void)
{
	HAL_ADC_Init(&hadc1); //on init l'adc
	HAL_ADCEx_Calibration_Start(&hadc1);

	debugTerminalCls(); //pour clear teraterm

	i2cEnhancedScanBus(&hi2c1);

	HAL_Delay(200);
	if(aht2xInit(&aht21, &hi2c1)){
		debugLogErrorTrace("Init Fail"); //init le aht21
		return;
	}

	HAL_Delay(200);
	if(aht2xStartMeasure(&aht21)){
		debugLogErrorTrace("Start Measure Fail"); // start les mesures
		return;
	}

	HAL_Delay(200);
	if(aht2xReadRawData(&aht21)){
		debugLogErrorTrace("Read Raw Data Fail"); //lire les mesures
		return;
	}
	aht2xProcessRawData(&aht21);

	/*HAL_Delay(200);
	if(aht2xStartAndPolMeasure(&aht21, 250)){ //time out en dessous de 250 pas possible
		debugLogErrorTrace("Start and Pol Measure Fail");
		return;
	}*/

	if (nrf_init(&nrf, &nrfConfig) != NRF_OK)
		debugLogErrorTrace("NRF24L01 Init Fail !!!");
	else
		debugLogInfoTrace("NRF24L01 Init OK :-)");

	HAL_Delay(1000);
}

void userAppTask(void)
{

	static UserAppTaskState_t state = USER_APP_TASK_STATE_INIT;
	//static uint8_t txData[sizeof(nrfRxBuffer)] = {0};
	static uint32_t nextSendTick = 0;
	uint32_t localTick = HAL_GetTick();
	switch (state)
	{
	case USER_APP_TASK_STATE_INIT:
		state = USER_APP_TASK_STATE_IDLE; //machine a etat
		break;

	case USER_APP_TASK_STATE_IDLE:
		if (localTick < nextSendTick)
			break;
		nextSendTick += 4000; //le temps entre chaque envoi de trame

		HAL_ADC_Start(&hadc1); //start adc
		HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY); //conversion
		IV_fab(); //initvector
		remplissage(); //on remplit la trame des parties découpées en bas

		nrf_send_packet(&nrf, &trame); //on envoi la trame
		count += 1;

		if (nrf.tx_result != NRF_OK)
			debugLogInfo("on est bloque dans le if");
		else
		{
			debugLogInfo("on envoi trame"); //les 4 lignes la c'est pour du debug sur teraterm
			debugLogInfo("vbat : %d", trame.VBAT);
			debugLogInfo("TS : %d", InitVector.timestamp);
			debugLogInfo("nodeID1 : %d", InitVector.NoneID1);
		}
		break;

	default:
		break;
	}
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
	switch (GPIO_Pin)
	{
	case NRF_INT_Pin:
		nrf_irq_handler(&nrf);
		break;
	default:
		break;
	}
}

void nrf_packet_received_callback(nrf24l01 *dev, uint8_t *data)
{
	// default implementation (__weak) is used in favor of nrf_receive_packet
//	debugLogWarningTrace("");
	dev->rx_busy = 0;
}

void remplissage(void){

	//**************  Init Vector ****************************

	trame.timestamp = HAL_GetTick(); //pour avoir le timestamp
	trame.NoneID1 = 10; //le 10 correspond a notre poste

	//**************  Payload  ****************************

	trame.FrameCounter = count;
	trame.NoneID2 = 10; //pareil
	trame.len = 07;
	trame.FrameType = 01;

	trame.temp = aht2xGetTemperature(&aht21, 0, 0); //fonctions dans le aht2x.c
	trame.hum = aht2xGetHumidity(&aht21, 0, 0); //fonctions dans le aht2x.c
	trame.VBAT = HAL_ADC_GetValue(&hadc1)*0.0805; //adc
	trame.QAIR = 77; //valeur aleatoire on a pas le ens160
	trame.C02 = 7777; //pareil

	 AES_init_ctx_iv(&ctx, key, (uint8_t*)&InitVector); //pour l'init vector en timestamp + NodeID + timestamp + NodeID
	 AES_CBC_encrypt_buffer(&ctx, (uint8_t*)&trame+8, 16); // crypter la trame
}

void IV_fab(void){ //creer le initvector (en timestamp + NodeID + timestamp + NodeID)

	InitVector.timestamp = HAL_GetTick(); //pareil
	InitVector.NoneID1 = 10; //pareil qu'en haut
	InitVector.timestamp2 = InitVector.timestamp;
	InitVector.NoneID12 = InitVector.NoneID1;
}

