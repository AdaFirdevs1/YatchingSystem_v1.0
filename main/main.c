#include "usrGeneral.h"
#include "usrDefinePassword.h"

void app_main()
{
    waveshare_esp32_s3_rgb_lcd_init();
    initCAN();

    //simpleCANTest();

    startCanCommunication();

    ESP_LOGI("main", "Display LVGL demos");

    if (lvgl_port_lock(-1))
    {
        usrGraphicalInterface_init();
        lvgl_port_unlock();
    }
}

// void simpleCANTest(void)
// {
//     twai_stop();
//     twai_driver_uninstall();
//     vTaskDelay(pdMS_TO_TICKS(500));

//     initCAN();

//     twai_status_info_t status_info;
//     twai_get_status_info(&status_info);
//     ESP_LOGI("main", "After restart - CAN State: %d", status_info.state);

//     vTaskDelay(pdMS_TO_TICKS(5000));
//     twai_get_status_info(&status_info);
//     ESP_LOGI("main", "After 5 seconds - CAN State: %d", status_info.state);
// }