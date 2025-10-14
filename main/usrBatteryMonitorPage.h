#ifndef _USR_BATTERY_MONITOR_PAGE_H_
#define _USR_BATTERY_MONITOR_PAGE_H_

#include "lvgl.h"
#include "usrGeneral.h"

// Battery monitor data structure
typedef struct
{
    uint8_t percentage;
    float voltage;
    float current;
    float temperature;
    const char *status;
} battery_data_t;

// Battery monitor page structure
typedef struct
{
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *arc_meters[4];
    lv_obj_t *arc_labels[4];
    lv_obj_t *value_labels[4];      // Artık sadece yüzde gösterecek
    lv_obj_t *voltage_labels[4];    // YENİ: Voltaj bilgileri için
    lv_obj_t *status_label;
    battery_data_t battery_data[4];
    bool is_active;
    lv_timer_t *update_timer;
} usrBatteryMonitorPage_t;

// Function declarations
void usrBatteryMonitorPage_init(lv_obj_t *parent);
void usrBatteryMonitorPage_show(void);
void usrBatteryMonitorPage_hide(void);
void usrBatteryMonitorPage_destroy(void);
void usrBatteryMonitorPage_updateBattery(int battery_index, uint8_t percentage, float voltage, float current, float temperature);
void usrBatteryMonitorPage_setStatus(const char *status);
bool usrBatteryMonitorPage_isActive(void);

void usrBatteryMonitorPage_refreshBatteryNames(void);

// Real sensor data collection functions
void usrBatteryMonitorPage_startDataCollection(void);
void usrBatteryMonitorPage_stopDataCollection(void);

// CAN data processing function
void usrBatteryMonitorPage_processCanData(uint32_t can_id, uint32_t data);

// Legacy simulation functions (for backward compatibility)
void usrBatteryMonitorPage_startSimulation(void);
void usrBatteryMonitorPage_stopSimulation(void);

void usrBatteryMonitorPage_setTestMode(bool enable);
bool usrBatteryMonitorPage_getTestMode(void);

#endif /* _USR_BATTERY_MONITOR_PAGE_H_ */