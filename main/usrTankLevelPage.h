#ifndef USR_TANK_LEVEL_PAGE_H
#define USR_TANK_LEVEL_PAGE_H

#include "lvgl.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sensor type enumeration
typedef enum {
    SENSOR_TYPE_0_190 = 0,
    SENSOR_TYPE_30_240 = 1
} sensor_type_t;

// Tank sensor data structure
typedef struct {
    uint16_t adc_value;
    float level_percentage;
    float volume_liters;
    uint16_t tank_capacity;
    sensor_type_t sensor_type;
    bool is_connected;
    bool is_configured;
    uint32_t last_data_time;
    char tank_name[16];
} tank_sensor_data_t;

// Main tank page structure
typedef struct {
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *status_label;
    
    // Tank widgets (8 tanks max)
    lv_obj_t *tank_containers[8];
    lv_obj_t *tank_charts[8];
    lv_obj_t *tank_labels[8];
    lv_obj_t *tank_value_labels[8];
    lv_obj_t *tank_status_labels[8];
    
    // Tank data
    tank_sensor_data_t tank_data[8];
    
    // Configuration popup
    lv_obj_t *modal_bg;
    lv_obj_t *input_popup;
    lv_obj_t *keyboard;
    lv_obj_t *ta_capacity;
    lv_obj_t *sensor_type_dropdown;
    int current_tank_index;
    
    // Timers and states
    lv_timer_t *update_timer;
    bool is_active;
    bool getTankRequest;
} usrTankLevelPage_t;

// Function prototypes
void usrTankLevelPage_init(lv_obj_t *parent);
void usrTankLevelPage_show(void);
void usrTankLevelPage_hide(void);
void usrTankLevelPage_destroy(void);
bool usrTankLevelPage_isActive(void);
void usrTankLevelPage_setStatus(const char *status);

// Data processing functions
void usrTankLevelPage_processCanData(uint32_t can_id, uint32_t data);
void usrTankLevelPage_updateTankData(int tank_index, uint16_t adc_value);
void usrTankLevelPage_updateTankDisplay(int tank_index);

// Widget management
void usrTankLevelPage_createTankWidget(int tank_index);
void usrTankLevelPage_destroyTankWidget(int tank_index);

// Configuration functions
void usrTankLevelPage_showConfigPopup(int tank_index);
void usrTankLevelPage_saveTankConfig(int tank_index, uint16_t capacity, sensor_type_t sensor_type);

// Data collection control
void usrTankLevelPage_startDataCollection(void);
void usrTankLevelPage_stopDataCollection(void);

// Utility functions
void usrTankLevelPage_clearAllConfig(void);

#ifdef __cplusplus
}
#endif

#endif // USR_TANK_LEVEL_PAGE_H