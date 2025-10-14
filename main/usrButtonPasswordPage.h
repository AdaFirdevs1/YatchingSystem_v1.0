#ifndef USR_BUTTON_PASSWORD_PAGE_H
#define USR_BUTTON_PASSWORD_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Password callback function type
typedef void (*button_password_callback_t)(void);

/**
 * @brief Initialize the button password page
 * 
 * @param parent Parent LVGL object
 */
void usrButtonPasswordPage_init(lv_obj_t *parent);

/**
 * @brief Show the button password page
 * 
 * @param success_callback Callback to execute when password is correct
 * @param cancel_callback Callback to execute when cancel is pressed
 */
void usrButtonPasswordPage_show(button_password_callback_t success_callback, button_password_callback_t cancel_callback);

/**
 * @brief Hide the button password page
 */
void usrButtonPasswordPage_hide(void);

/**
 * @brief Destroy the button password page and free resources
 */
void usrButtonPasswordPage_destroy(void);

/**
 * @brief Check if the button password page is currently visible
 * 
 * @return true if visible, false otherwise
 */
bool usrButtonPasswordPage_isVisible(void);

#ifdef __cplusplus
}
#endif

#endif // USR_BUTTON_PASSWORD_PAGE_H