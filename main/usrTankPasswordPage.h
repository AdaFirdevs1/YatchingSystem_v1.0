#ifndef USR_TANK_PASSWORD_PAGE_H
#define USR_TANK_PASSWORD_PAGE_H

#include "lvgl.h"

// Callback types
typedef void (*tank_password_callback_t)(void);

/**
 * @brief Initialize the tank password verification page
 * 
 * @param parent Parent object (usually the screen)
 */
void usrTankPasswordPage_init(lv_obj_t *parent);

/**
 * @brief Show the password page with callbacks
 * 
 * @param success_callback Function to call when password is correct
 * @param cancel_callback Function to call when cancelled or too many attempts
 */
void usrTankPasswordPage_show(tank_password_callback_t success_callback, tank_password_callback_t cancel_callback);

/**
 * @brief Hide the password page
 */
void usrTankPasswordPage_hide(void);

/**
 * @brief Destroy the password page and free resources
 */
void usrTankPasswordPage_destroy(void);

/**
 * @brief Check if password page is currently visible
 * 
 * @return true if visible, false otherwise
 */
bool usrTankPasswordPage_isVisible(void);

#endif // USR_TANK_PASSWORD_PAGE_H