#ifndef _USR_CAN_H_
#define _USR_CAN_H_

#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define CAN_RX_PIN  GPIO_NUM_19
#define CAN_TX_PIN  GPIO_NUM_20  

void initCAN(void);

esp_err_t sendCanHeader(uint32_t m_canID, uint32_t m_value);

esp_err_t receiveCANMessage(uint32_t *id, uint8_t *data, size_t *dataLen);

void startCanCommunication(void);

void processQuadroShuntData(uint32_t can_id, uint32_t data);
void processMainShuntData(uint32_t can_id, uint32_t data);
void processSensorData(uint32_t can_id, uint32_t data);

#endif /*_USR_CAN_H_*/