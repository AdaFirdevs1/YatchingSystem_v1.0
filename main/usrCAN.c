#include "usrGeneral.h"
#include "usrBatteryMonitorPage.h"

static const char *tag = "usrCan";
static TaskHandle_t s_canReceiveTaskHandle;

void initCAN(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t ret = twai_driver_install(&g_config, &t_config, &f_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE("usrCanInit", "CAN driver install failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI("usrCanInit", "CAN driver installed successfully");

    ret = twai_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE("usrCanInit", "CAN start failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI("usrCanInit", "CAN started successfully");

    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    ESP_LOGI("usrCanInit", "CAN State: %d", status_info.state);
}

esp_err_t sendCanHeader(uint32_t m_canID, uint32_t m_value)
{
    twai_message_t txMessage;
    uint8_t canData[8] = {0};

    twai_status_info_t status_info;
    esp_err_t canState = twai_get_status_info(&status_info);

    if (canState != ESP_OK || status_info.state != TWAI_STATE_RUNNING)
    {
        if (status_info.state == TWAI_STATE_STOPPED)
        {
            ESP_LOGW(tag, "CAN Stopped - Restarting");
            esp_err_t ret = twai_start();
            if (ret != ESP_OK)
            {
                ESP_LOGE(tag, "CAN restart failed: %s", esp_err_to_name(ret));
                return ESP_FAIL;
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        else if (status_info.state == TWAI_STATE_BUS_OFF)
        {
            ESP_LOGW(tag, "CAN Bus-off - Recovery");
            twai_initiate_recovery();
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        twai_get_status_info(&status_info);
        if (status_info.state != TWAI_STATE_RUNNING)
        {
            ESP_LOGE(tag, "CAN Not Ready - State: %d", status_info.state);
            return ESP_FAIL;
        }
    }

    txMessage.identifier = m_canID;
    txMessage.data_length_code = 8;
    txMessage.flags = TWAI_MSG_FLAG_NONE;

    canData[0] = (uint8_t)(m_value >> 24);
    canData[1] = (uint8_t)(m_value >> 16);
    canData[2] = (uint8_t)(m_value >> 8);
    canData[3] = (uint8_t)(m_value);
    canData[4] = 0x00;
    canData[5] = 0x00;
    canData[6] = 0x00;
    canData[7] = 0x00;

    for (int i = 0; i < 8; i++)
    {
        txMessage.data[i] = canData[i];
    }

    esp_err_t status = twai_transmit(&txMessage, pdMS_TO_TICKS(1000));

    if (status != ESP_OK)
    {
        ESP_LOGE(tag, "CAN TX FAILED - Status: %d Error:0x%X", status, status);
        return status;
    }

    return ESP_OK;
}

esp_err_t receiveCANMessage(uint32_t *id, uint8_t *data, size_t *dataLen)
{
    twai_message_t m_message;

    if (twai_receive(&m_message, 10) == ESP_OK)
    {
        *id = m_message.identifier;
        *dataLen = m_message.data_length_code;

        for (size_t i = 0; i < *dataLen; i++)
        {
            data[i] = m_message.data[i];
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

static void canReceiveTask(void *arg)
{
    ESP_LOGI(tag, "CAN Receive Task started");
    uint32_t id;
    uint8_t data[8];
    size_t dataLen;
    uint32_t receivedValue = 0;

    while (1)
    {
        if (receiveCANMessage(&id, data, &dataLen) == ESP_OK)
        {
            receivedValue = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];

            if (id >= 0x100 && id <= 0x199)
            {
                processQuadroShuntData(id, receivedValue);
            }
            if (id >= 0x200 && id <= 0x299)
            {
                processMainShuntData(id, receivedValue);
            }

            if (id >= 0x300 && id <= 0x399)
            {
                processSensorData(id, receivedValue);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void processSensorData(uint32_t can_id, uint32_t data)
{
    if (can_id >= 0x300 && can_id <= 0x303)
    {
        usrBatteryMonitorPage_processCanData(can_id, data);
        return;
    }

    if (can_id >= 0x310 && can_id <= 0x313)
    {
        // TODO: Process 4-20mA sensor data when that page is ready
        ESP_LOGI(tag, "4-20mA Sensor %ld: ADC=%ld", can_id - 0x310 + 1, data);
        return;
    }

    if ((can_id >= 0x320 && can_id <= 0x323) || (can_id >= 0x330 && can_id <= 0x333))
    {
        usrTankLevelPage_processCanData(can_id, data);
        return;
    }
}

void processQuadroShuntData(uint32_t can_id, uint32_t data)
{
    if (can_id >= 0x180 && can_id <= 0x183)
    {
        usrShuntPage_processCanData(can_id, data);
        return;
    }
    ESP_LOGW(tag, "Unknown Quadro Shunt ID: 0x%lX", can_id);
}

void processMainShuntData(uint32_t can_id, uint32_t data)
{
    if (can_id == 0x280)
    {
        usrShuntPage_processCanData(can_id, data);
        return;
    }
    // ESP_LOGW(tag, "Unknown Main Shunt ID: 0x%lX", can_id);
}

void startCanCommunication(void)
{
    xTaskCreate(canReceiveTask, "CAN Receive", 4096, NULL, 5, &s_canReceiveTaskHandle);
    ESP_LOGI(tag, "CAN communication started");
}