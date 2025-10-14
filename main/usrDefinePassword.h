#ifndef USR_DEFINE_PASSWORD_H
#define USR_DEFINE_PASSWORD_H

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Password configuration
#define PASSWORD_MIN_LENGTH 4
#define PASSWORD_MAX_LENGTH 16
#define PASSWORD_NVS_KEY "user_password"
#define PASSWORD_NVS_NAMESPACE "password"

// Define password states
typedef enum {
    DEFINE_PASSWORD_STATE_INPUT,
    DEFINE_PASSWORD_STATE_CONFIRM,
    DEFINE_PASSWORD_STATE_SUCCESS,
    DEFINE_PASSWORD_STATE_ERROR
} define_password_state_t;

// Password validation callback type
typedef void (*password_defined_callback_t)(bool success);

/**
 * @brief Initialize the password definition password
 * @param parent Parent container to create the password in
 * @param callback Callback function called when password is defined (can be NULL)
 */
void usrDefinePassword_init(lv_obj_t *parent, password_defined_callback_t callback);

/**
 * @brief Show the password definition password
 */
void usrDefinePassword_show(void);

/**
 * @brief Hide the password definition password
 */
void usrDefinePassword_hide(void);

/**
 * @brief Destroy the password definition password and free resources
 */
void usrDefinePassword_destroy(void);

/**
 * @brief Check if password is already defined in NVS
 * @return true if password exists, false otherwise
 */
bool usrDefinePassword_isPasswordDefined(void);

/**
 * @brief Verify entered password against stored password
 * @param password Password to verify
 * @return true if password matches, false otherwise
 */
bool usrDefinePassword_verifyPassword(const char *password);

/**
 * @brief Get password strength indicator
 * @param password Password to check
 * @return Strength level (0-3: weak to strong)
 */
int usrDefinePassword_getPasswordStrength(const char *password);

/**
 * @brief Clear stored password from NVS (for reset functionality)
 * @return true if successfully cleared, false otherwise
 */
bool usrDefinePassword_clearPassword(void);

#ifdef __cplusplus
}
#endif

#endif // USR_DEFINE_PASSWORD_H