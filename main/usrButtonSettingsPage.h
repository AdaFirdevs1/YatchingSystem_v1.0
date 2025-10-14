#ifndef USR_BUTTON_SETTINGS_PAGE_H
#define USR_BUTTON_SETTINGS_PAGE_H

#include "lvgl.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

// Button settings structure
typedef struct {
    // UI Components
    lv_obj_t *page_container;
    lv_obj_t *back_button;
    lv_obj_t *title_label;
    lv_obj_t *select_all_button;
    lv_obj_t *deselect_all_button;
    lv_obj_t *save_button;
    lv_obj_t *reset_button;
    lv_obj_t *status_label;
    lv_obj_t *keyboard;
    
    // Button widgets (6 buttons)
    lv_obj_t *button_containers[6];
    lv_obj_t *button_name_labels[6];
    lv_obj_t *button_name_textareas[6];
    
    // State variables
    bool is_active;
    bool is_modified;
    lv_obj_t *current_textarea;
    void (*back_callback)(void);
    
} usrButtonSettingsPage_t;

// Function declarations
void usrButtonSettingsPage_init(lv_obj_t *parent);
void usrButtonSettingsPage_show(void);
void usrButtonSettingsPage_hide(void);
void usrButtonSettingsPage_destroy(void);
bool usrButtonSettingsPage_isActive(void);
void usrButtonSettingsPage_setBackCallback(void (*callback)(void));
void usrButtonSettingsPage_loadButtonNames(char button_names[6][11]);
void usrButtonSettingsPage_saveButtonNames(void);



// Debug functions
void usrButtonSettingsPage_debugSelection(void);
void usrButtonSettingsPage_testNVSReadWrite(void);

// YENI: Public NVS erişim fonksiyonları
esp_err_t load_button_names_from_nvs(char button_names[6][11]);
esp_err_t load_selected_buttons_from_nvs(bool selected_buttons[6]);
esp_err_t save_button_names_to_nvs(char button_names[6][11]);



/**
 * @brief Initialize NVS flash storage
 * @return ESP_OK on success
 */
esp_err_t usrButtonSettingsPage_initNVS(void);

#endif // USR_BUTTON_SETTINGS_PAGE_H