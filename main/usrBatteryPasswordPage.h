#ifndef USR_BATTERY_PASSWORD_PAGE_H
#define USR_BATTERY_PASSWORD_PAGE_H

#include "lvgl.h"
#include <stdbool.h>

// Callback function type for password events
typedef void (*battery_password_callback_t)(void);

/**
 * @brief Initialize the battery password page
 * @param parent Parent object (usually screen or container)
 */
void usrBatteryPasswordPage_init(lv_obj_t *parent);

/**
 * @brief Show the battery password page with callbacks
 * @param success_callback Function to call when password is correct
 * @param cancel_callback Function to call when user cancels
 */
void usrBatteryPasswordPage_show(battery_password_callback_t success_callback, 
                                battery_password_callback_t cancel_callback);

/**
 * @brief Hide the battery password page
 */
void usrBatteryPasswordPage_hide(void);

/**
 * @brief Destroy the battery password page and free resources
 */
void usrBatteryPasswordPage_destroy(void);

/**
 * @brief Check if the battery password page is currently visible
 * @return true if visible, false otherwise
 */
bool usrBatteryPasswordPage_isVisible(void);

#endif // USR_BATTERY_PASSWORD_PAGE_H