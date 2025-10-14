#ifndef USR_SHUNT_SETTINGS_PAGE_H
#define USR_SHUNT_SETTINGS_PAGE_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of shunts (1 Main + 4 Quadro)
#define MAX_SHUNTS 5

// Main structure for shunt settings page
typedef struct {
    // Main container and UI elements
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *back_button;
    lv_obj_t *select_all_button;
    lv_obj_t *deselect_all_button;
    lv_obj_t *save_button;
    lv_obj_t *reset_button;
    lv_obj_t *status_label;
    lv_obj_t *keyboard;
    
    // Shunt name editing elements (arrays for 5 shunts)
    lv_obj_t *shunt_name_containers[MAX_SHUNTS];     // Container for each shunt
    lv_obj_t *shunt_name_labels[MAX_SHUNTS];         // Labels for shunt names
    lv_obj_t *shunt_name_textareas[MAX_SHUNTS];      // Text areas for editing names
    
    
    // State variables
    bool is_active;                                   // Is page currently visible
    bool is_modified;                                 // Has user made changes
    lv_obj_t *current_textarea;                       // Currently focused textarea
    
    // Callback function
    void (*back_callback)(void);                      // Callback for back button

    lv_obj_t *header_container;        // Sabit header için
    lv_obj_t *scrollable_content;      // Kaydırılabilir içerik için
    
} usrShuntSettingsPage_t;

// Function declarations

/**
 * @brief Initialize the shunt settings page
 * @param parent Parent LVGL object to attach the page to
 */
void usrShuntSettingsPage_init(lv_obj_t *parent);

/**
 * @brief Show the shunt settings page
 */
void usrShuntSettingsPage_show(void);

/**
 * @brief Hide the shunt settings page
 */
void usrShuntSettingsPage_hide(void);

/**
 * @brief Destroy the shunt settings page and free resources
 */
void usrShuntSettingsPage_destroy(void);

/**
 * @brief Check if the shunt settings page is currently active
 * @return true if active, false otherwise
 */
bool usrShuntSettingsPage_isActive(void);

/**
 * @brief Set callback function for back button
 * @param callback Function to call when back button is pressed
 */
void usrShuntSettingsPage_setBackCallback(void (*callback)(void));

// Shunt name management functions

/**
 * @brief Load shunt names from NVS storage
 * @param shunt_names Array to store loaded names [5][16]
 */
void usrShuntSettingsPage_loadShuntNames(char shunt_names[5][16]);

/**
 * @brief Save current shunt names and settings
 */
void usrShuntSettingsPage_saveShuntNames(void);

// Shunt selection management functions

/**
 * @brief Get pointer to selected shunts array
 * @return Pointer to bool array of selected states
 */
bool* usrShuntSettingsPage_getSelectedShunts(void);

/**
 * @brief Set selection state for a specific shunt
 * @param shunt_index Index of shunt (0=Main, 1-4=Quadro 1-4)
 * @param selected true to show in main page, false to hide
 */
void usrShuntSettingsPage_setSelectedShunt(int shunt_index, bool selected);

/**
 * @brief Save selected shunts configuration to NVS
 */
void usrShuntSettingsPage_saveSelectedShunts(void);

/**
 * @brief Load selected shunts configuration from NVS
 */
void usrShuntSettingsPage_loadSelectedShunts(void);

// Debug and utility functions

/**
 * @brief Debug function to print current selection states
 */
void usrShuntSettingsPage_debugSelection(void);

/**
 * @brief Test NVS read/write functionality
 */
void usrShuntSettingsPage_testNVSReadWrite(void);

#ifdef __cplusplus
}
#endif

#endif // USR_SHUNT_SETTINGS_PAGE_H