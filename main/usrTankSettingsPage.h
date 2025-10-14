#ifndef USR_TANK_SETTINGS_PAGE_H
#define USR_TANK_SETTINGS_PAGE_H

#include "lvgl.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Settings page structure
typedef struct {
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *back_button;
    lv_obj_t *scroll_container;
    lv_obj_t *save_button;
    lv_obj_t *reset_button;
    lv_obj_t *status_label;
    
    // Tank name inputs (8 tanks)
    lv_obj_t *tank_name_containers[8];
    lv_obj_t *tank_name_labels[8];
    lv_obj_t *tank_name_textareas[8];
    
    // Keyboard for input
    lv_obj_t *keyboard;
    lv_obj_t *current_textarea;
    
    bool is_active;
    bool is_modified;
    
    // Callback for page navigation
    void (*back_callback)(void);

} usrTankSettingsPage_t;

// Function prototypes
void usrTankSettingsPage_init(lv_obj_t *parent);
void usrTankSettingsPage_show(void);
void usrTankSettingsPage_hide(void);
void usrTankSettingsPage_destroy(void);
bool usrTankSettingsPage_isActive(void);
void usrTankSettingsPage_setBackCallback(void (*callback)(void));
void usrTankSettingsPage_loadTankNames(char tank_names[8][16]);
void usrTankSettingsPage_saveTankNames(void);


void usrTankSettingsPage_debugScroll(void);

void usrTankSettingsPage_debugSelection(void);

void usrTankSettingsPage_testNVSReadWrite(void);

#ifdef __cplusplus
}
#endif

#endif // USR_TANK_SETTINGS_PAGE_H