#ifndef USR_SHUNT_PASSWORD_PAGE_H
#define USR_SHUNT_PASSWORD_PAGE_H

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback function type for password success/cancel events
 */
typedef void (*shunt_password_callback_t)(void);

/**
 * @brief Initialize the shunt password page
 * 
 * @param parent Parent object to attach the password page to
 */
void usrShuntPasswordPage_init(lv_obj_t *parent);

/**
 * @brief Show the shunt password page
 * 
 * @param success_callback Callback to call when password is correct
 * @param cancel_callback Callback to call when user cancels or password is incorrect
 */
void usrShuntPasswordPage_show(shunt_password_callback_t success_callback, shunt_password_callback_t cancel_callback);

/**
 * @brief Hide the shunt password page
 */
void usrShuntPasswordPage_hide(void);

/**
 * @brief Destroy the shunt password page and free resources
 */
void usrShuntPasswordPage_destroy(void);

/**
 * @brief Check if the password page is currently visible
 * 
 * @return true if visible, false otherwise
 */
bool usrShuntPasswordPage_isVisible(void);

#ifdef __cplusplus
}
#endif

#endif /* USR_SHUNT_PASSWORD_PAGE_H */