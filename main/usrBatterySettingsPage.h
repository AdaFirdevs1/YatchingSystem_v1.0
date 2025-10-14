// usrBatterySettingsPage.h - Battery Settings Page Header

#ifndef USR_BATTERY_SETTINGS_PAGE_H
#define USR_BATTERY_SETTINGS_PAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lvgl.h"

// Battery settings page structure
typedef struct {
    // Main page container
    lv_obj_t *page_container;
    
    // Top navigation elements
    lv_obj_t *back_button;
    lv_obj_t *title_label;
    
    // Battery name widgets (4 batteries)
    lv_obj_t *battery_name_containers[4];
    lv_obj_t *battery_name_labels[4];
    lv_obj_t *battery_name_textareas[4];
    
    
    // Bottom controls
    lv_obj_t *save_button;
    lv_obj_t *reset_button;
    lv_obj_t *status_label;
    
    // Keyboard
    lv_obj_t *keyboard;
    lv_obj_t *current_textarea;
    
    // State variables
    bool is_active;
    bool is_modified;
    bool selected_batteries[4];
    
    // Callback function
    void (*back_callback)(void);
    
} usrBatterySettingsPage_t;

// Function declarations

/**
 * @brief Initialize the battery settings page
 * @param parent Parent object to attach the page to
 */
void usrBatterySettingsPage_init(lv_obj_t *parent);

/**
 * @brief Show the battery settings page
 */
void usrBatterySettingsPage_show(void);

/**
 * @brief Hide the battery settings page
 */
void usrBatterySettingsPage_hide(void);

/**
 * @brief Destroy the battery settings page and free memory
 */
void usrBatterySettingsPage_destroy(void);

/**
 * @brief Check if the battery settings page is currently active
 * @return true if active, false if not
 */
bool usrBatterySettingsPage_isActive(void);

/**
 * @brief Set the back button callback function
 * @param callback Function to call when back button is pressed
 */
void usrBatterySettingsPage_setBackCallback(void (*callback)(void));

/**
 * @brief Load battery names from NVS
 * @param battery_names Array to store the loaded battery names
 */
void usrBatterySettingsPage_loadBatteryNames(char battery_names[4][16]);

/**
 * @brief Save current battery names to NVS (trigger save button functionality)
 */
void usrBatterySettingsPage_saveBatteryNames(void);

/**
 * @brief Get pointer to selected batteries array
 * @return Pointer to bool array indicating which batteries are selected
 */
bool* usrBatterySettingsPage_getSelectedBatteries(void);

/**
 * @brief Set selection state for a specific battery
 * @param battery_index Index of the battery (0-3)
 * @param selected true to select, false to deselect
 */
void usrBatterySettingsPage_setSelectedBattery(int battery_index, bool selected);

/**
 * @brief Save selected batteries state to NVS
 */
void usrBatterySettingsPage_saveSelectedBatteries(void);

/**
 * @brief Load selected batteries state from NVS
 */
void usrBatterySettingsPage_loadSelectedBatteries(void);

/**
 * @brief Debug function to print current selection state
 */
void usrBatterySettingsPage_debugSelection(void);

/**
 * @brief Test NVS read/write functionality
 */
void usrBatterySettingsPage_testNVSReadWrite(void);

#endif // USR_BATTERY_SETTINGS_PAGE_H