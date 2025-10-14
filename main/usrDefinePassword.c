#include "usrDefinePassword.h"
#include "usrGeneralDefines.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <ctype.h>

static const char *s_tag = "usrDefinePassword";

typedef struct {
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *instruction_label;
    lv_obj_t *password_textarea;
    lv_obj_t *confirm_textarea;
    lv_obj_t *strength_label;
    lv_obj_t *strength_bar;
    lv_obj_t *message_label;
    lv_obj_t *keyboard;
    lv_obj_t *confirm_btn;
    
    char password_buffer[PASSWORD_MAX_LENGTH + 1];
    char confirm_buffer[PASSWORD_MAX_LENGTH + 1];
    define_password_state_t current_state;
    password_defined_callback_t callback;
    bool is_initialized;
    bool is_visible;
    bool keyboard_visible;
} usrDefinePassword_t;

static usrDefinePassword_t define_password = {0};

// Password strength colors
static const lv_color_t strength_colors[] = {
    LV_COLOR_MAKE(255, 0, 0),    // Red - Weak
    LV_COLOR_MAKE(255, 165, 0),  // Orange - Fair
    LV_COLOR_MAKE(255, 255, 0),  // Yellow - Good
    LV_COLOR_MAKE(0, 255, 0)     // Green - Strong
};

static const char *strength_texts[] = {
    "Weak",
    "Fair",
    "Good",
    "Strong"
};

// Function prototypes
static void textarea_event_cb(lv_event_t *e);
static void button_event_cb(lv_event_t *e);
static void keyboard_event_cb(lv_event_t *e);
static void background_event_cb(lv_event_t *e);
static void update_password_strength(void);
static void update_ui_state(void);
static bool save_password_to_nvs(const char *password);
static bool load_password_from_nvs(char *buffer, size_t buffer_size);
static void show_message(const char *message, lv_color_t color);
static void clear_inputs(void);
static void hide_keyboard(void);
static void show_keyboard(lv_obj_t *target_textarea);

void usrDefinePassword_init(lv_obj_t *parent, password_defined_callback_t callback)
{
    if (define_password.is_initialized) {
        ESP_LOGW(s_tag, "Define password already initialized");
        return;
    }

    define_password.callback = callback;
    define_password.current_state = DEFINE_PASSWORD_STATE_INPUT;
    define_password.keyboard_visible = false;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create main page container - FULL SCREEN
    define_password.page_container = lv_obj_create(parent);
    lv_obj_set_size(define_password.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(define_password.page_container, -20, -20);
    lv_obj_set_style_bg_color(define_password.page_container, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_bg_opa(define_password.page_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(define_password.page_container, 0, 0);
    lv_obj_set_style_pad_all(define_password.page_container, 0, 0);
    lv_obj_clear_flag(define_password.page_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Add background click event to hide keyboard
    lv_obj_add_event_cb(define_password.page_container, background_event_cb, LV_EVENT_CLICKED, NULL);

    // Title label
    define_password.title_label = lv_label_create(define_password.page_container);
    lv_label_set_text(define_password.title_label, "PASSWORD SETUP");
    lv_obj_set_style_text_font(define_password.title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(define_password.title_label, lv_color_white(), 0);
    lv_obj_align(define_password.title_label, LV_ALIGN_CENTER, 0, -170);

    // Instruction label
    define_password.instruction_label = lv_label_create(define_password.page_container);
    lv_label_set_text(define_password.instruction_label, 
        "Create a password to secure your settings.\n"
        "Password must be 4-16 characters long.");
    lv_obj_set_style_text_font(define_password.instruction_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(define_password.instruction_label, lv_color_make(200, 200, 200), 0);
    lv_obj_set_style_text_align(define_password.instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(define_password.instruction_label, LV_ALIGN_CENTER, 0, -120);

    // Password input textarea
    define_password.password_textarea = lv_textarea_create(define_password.page_container);
    lv_obj_set_size(define_password.password_textarea, 250, 40);
    lv_textarea_set_placeholder_text(define_password.password_textarea, "Enter password...");
    lv_textarea_set_password_mode(define_password.password_textarea, true);
    lv_textarea_set_max_length(define_password.password_textarea, PASSWORD_MAX_LENGTH);
    lv_obj_align(define_password.password_textarea, LV_ALIGN_CENTER, 0, -60);
    lv_obj_add_event_cb(define_password.password_textarea, textarea_event_cb, LV_EVENT_ALL, NULL);

    // Confirm password textarea (initially hidden)
    define_password.confirm_textarea = lv_textarea_create(define_password.page_container);
    lv_obj_set_size(define_password.confirm_textarea, 250, 40);
    lv_textarea_set_placeholder_text(define_password.confirm_textarea, "Confirm password...");
    lv_textarea_set_password_mode(define_password.confirm_textarea, true);
    lv_textarea_set_max_length(define_password.confirm_textarea, PASSWORD_MAX_LENGTH);
    lv_obj_align(define_password.confirm_textarea, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(define_password.confirm_textarea, textarea_event_cb, LV_EVENT_ALL, NULL);
    //lv_obj_add_flag(define_password.confirm_textarea, LV_OBJ_FLAG_HIDDEN);

    // Password strength indicator - centered layout
    define_password.strength_label = lv_label_create(define_password.page_container);
    lv_label_set_text(define_password.strength_label, "Strength: ");
    lv_obj_set_style_text_font(define_password.strength_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(define_password.strength_label, lv_color_white(), 0);
    lv_obj_align(define_password.strength_label, LV_ALIGN_CENTER, -75, 30);

    // Strength bar
    define_password.strength_bar = lv_bar_create(define_password.page_container);
    lv_obj_set_size(define_password.strength_bar, 120, 8);
    lv_bar_set_range(define_password.strength_bar, 0, 3);
    lv_bar_set_value(define_password.strength_bar, 0, LV_ANIM_OFF);
    lv_obj_align(define_password.strength_bar, LV_ALIGN_CENTER, 45, 30);

    // Message label for feedback
    define_password.message_label = lv_label_create(define_password.page_container);
    lv_label_set_text(define_password.message_label, "");
    lv_obj_set_style_text_font(define_password.message_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(define_password.message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(define_password.message_label, LV_ALIGN_CENTER, 0, 90);

    // Confirm button - centered at bottom
    define_password.confirm_btn = lv_btn_create(define_password.page_container);
    lv_obj_set_size(define_password.confirm_btn, 120, 45);
    lv_obj_align(define_password.confirm_btn, LV_ALIGN_CENTER, -10, 100);
    lv_obj_set_style_bg_color(define_password.confirm_btn, lv_color_make(47, 127, 219), 0);
    lv_obj_add_event_cb(define_password.confirm_btn, button_event_cb, LV_EVENT_CLICKED, NULL);
    
    {
        lv_obj_t *confirm_label = lv_label_create(define_password.confirm_btn);
        lv_label_set_text(confirm_label, "CONFIRM");
        lv_obj_center(confirm_label);
    }

    // Create keyboard (initially hidden)
    define_password.keyboard = lv_keyboard_create(define_password.page_container);
    lv_obj_set_size(define_password.keyboard, LV_HOR_RES - 20, 180);
    lv_obj_align(define_password.keyboard, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(define_password.keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(define_password.keyboard, LV_OBJ_FLAG_HIDDEN);

    // Initialize buffers
    memset(define_password.password_buffer, 0, sizeof(define_password.password_buffer));
    memset(define_password.confirm_buffer, 0, sizeof(define_password.confirm_buffer));

    define_password.is_initialized = true;
    define_password.is_visible = false;

    // Hide by default
    lv_obj_add_flag(define_password.page_container, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(s_tag, "Password definition page initialized");
}

void usrDefinePassword_show(void)
{
    if (!define_password.is_initialized) {
        ESP_LOGW(s_tag, "Define password not initialized");
        return;
    }

    lv_obj_clear_flag(define_password.page_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(define_password.page_container); // Bring to front
    define_password.is_visible = true;
    
    define_password.current_state = DEFINE_PASSWORD_STATE_INPUT;
    clear_inputs();
    update_ui_state();
    hide_keyboard(); // Start with keyboard hidden

    ESP_LOGI(s_tag, "Password definition page shown");
}

void usrDefinePassword_hide(void)
{
    if (!define_password.is_initialized) return;

    lv_obj_add_flag(define_password.page_container, LV_OBJ_FLAG_HIDDEN);
    define_password.is_visible = false;
    hide_keyboard();

    ESP_LOGI(s_tag, "Password definition page hidden");
}

void usrDefinePassword_destroy(void)
{
    if (!define_password.is_initialized) return;

    if (define_password.page_container) {
        lv_obj_del(define_password.page_container);
    }

    memset(&define_password, 0, sizeof(usrDefinePassword_t));

    ESP_LOGI(s_tag, "Password definition page destroyed");
}

bool usrDefinePassword_isPasswordDefined(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(PASSWORD_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) return false;

    size_t required_size;
    err = nvs_get_str(nvs_handle, PASSWORD_NVS_KEY, NULL, &required_size);
    nvs_close(nvs_handle);

    return (err == ESP_OK && required_size > 0);
}

bool usrDefinePassword_verifyPassword(const char *password)
{
    if (!password) return false;

    char stored_password[PASSWORD_MAX_LENGTH + 1];
    if (!load_password_from_nvs(stored_password, sizeof(stored_password))) return false;

    return (strcmp(password, stored_password) == 0);
}

int usrDefinePassword_getPasswordStrength(const char *password)
{
    if (!password || strlen(password) < PASSWORD_MIN_LENGTH) return 0;

    int strength = 0;
    int len = strlen(password);
    bool has_lower = false, has_upper = false, has_digit = false, has_special = false;

    if (len >= 8) strength++;

    for (int i = 0; i < len; i++) {
        if (islower((unsigned char)password[i])) has_lower = true;
        else if (isupper((unsigned char)password[i])) has_upper = true;
        else if (isdigit((unsigned char)password[i])) has_digit = true;
        else has_special = true;
    }

    int char_types = has_lower + has_upper + has_digit + has_special;
    if (char_types >= 2) strength++;
    if (char_types >= 3) strength++;

    return strength > 3 ? 3 : strength;
}

bool usrDefinePassword_clearPassword(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(PASSWORD_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_erase_key(nvs_handle, PASSWORD_NVS_KEY);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error erasing password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error committing NVS: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(s_tag, "Password cleared from NVS");
    return true;
}

// Static functions

static void background_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED && target == define_password.page_container) {
        // Background clicked - hide keyboard and defocus textareas
        if (define_password.keyboard_visible) {
            hide_keyboard();
            
            // Remove focus from both textareas
            if (lv_obj_has_state(define_password.password_textarea, LV_STATE_FOCUSED)) {
                lv_obj_clear_state(define_password.password_textarea, LV_STATE_FOCUSED);
            }
            if (lv_obj_has_state(define_password.confirm_textarea, LV_STATE_FOCUSED)) {
                lv_obj_clear_state(define_password.confirm_textarea, LV_STATE_FOCUSED);
            }
            
            ESP_LOGI(s_tag, "Background clicked - keyboard hidden");
        }
    }
}

static void textarea_event_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        const char *text = lv_textarea_get_text(ta);
        if (ta == define_password.password_textarea) {
            strncpy(define_password.password_buffer, text, PASSWORD_MAX_LENGTH);
            define_password.password_buffer[PASSWORD_MAX_LENGTH] = '\0';
            update_password_strength();
        }
        else if (ta == define_password.confirm_textarea) {
            strncpy(define_password.confirm_buffer, text, PASSWORD_MAX_LENGTH);
            define_password.confirm_buffer[PASSWORD_MAX_LENGTH] = '\0';
        }
    }
    else if (code == LV_EVENT_FOCUSED) {
        show_keyboard(ta);
    }
}

static void button_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);

    if (btn == define_password.confirm_btn) {
        // İlk kontrol: şifre minimum uzunlukta mı?
        if (strlen(define_password.password_buffer) < PASSWORD_MIN_LENGTH) {
            show_message("Password too short!", lv_color_make(255, 0, 0));
            return;
        }
        
        // İkinci kontrol: confirm alanı boş mu?
        if (strlen(define_password.confirm_buffer) == 0) {
            show_message("Please fill confirm password field", lv_color_make(255, 255, 0));
            show_keyboard(define_password.confirm_textarea);
            lv_obj_add_state(define_password.confirm_textarea, LV_STATE_FOCUSED);
            return;
        }
        
        // Üçüncü kontrol: şifreler eşleşiyor mu?
        if (strcmp(define_password.password_buffer, define_password.confirm_buffer) != 0) {
            show_message("Passwords don't match!", lv_color_make(255, 0, 0));
            return;
        }
        
        // Her şey tamam, şifreyi kaydet
        if (save_password_to_nvs(define_password.password_buffer)) {
            define_password.current_state = DEFINE_PASSWORD_STATE_SUCCESS;
            show_message("Password saved successfully!", lv_color_make(0, 255, 0));
            hide_keyboard();
            if (define_password.callback) define_password.callback(true);
            ESP_LOGI(s_tag, "Password successfully defined and saved");
        } else {
            define_password.current_state = DEFINE_PASSWORD_STATE_ERROR;
            show_message("Error saving password!", lv_color_make(255, 0, 0));
            if (define_password.callback) define_password.callback(false);
        }
    }
}

static void keyboard_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        // Klavye ENTER tuşuna basıldığında confirm butonunu tetikle
        lv_event_send(define_password.confirm_btn, LV_EVENT_CLICKED, NULL);
    }
}

static void hide_keyboard(void)
{
    if (define_password.keyboard_visible) {
        lv_obj_add_flag(define_password.keyboard, LV_OBJ_FLAG_HIDDEN);
        define_password.keyboard_visible = false;
        lv_keyboard_set_textarea(define_password.keyboard, NULL);
        ESP_LOGD(s_tag, "Keyboard hidden");
    }
}

static void show_keyboard(lv_obj_t *target_textarea)
{
    if (!define_password.keyboard_visible) {
        lv_obj_clear_flag(define_password.keyboard, LV_OBJ_FLAG_HIDDEN);
        define_password.keyboard_visible = true;
        ESP_LOGD(s_tag, "Keyboard shown");
    }
    lv_keyboard_set_textarea(define_password.keyboard, target_textarea);
}

static void update_password_strength(void)
{
    int strength = usrDefinePassword_getPasswordStrength(define_password.password_buffer);
    lv_bar_set_value(define_password.strength_bar, strength, LV_ANIM_ON);
    lv_obj_set_style_bg_color(define_password.strength_bar, strength_colors[strength], LV_PART_INDICATOR);

    char str_text[50];
    snprintf(str_text, sizeof(str_text), "Strength: %s", strength_texts[strength]);
    lv_label_set_text(define_password.strength_label, str_text);
    lv_obj_set_style_text_color(define_password.strength_label, strength_colors[strength], 0);
}

static void update_ui_state(void)
{
    // Başlangıç instruction'ını ayarla - her iki alan da görünür olduğu için sabit
    lv_label_set_text(define_password.instruction_label,
        "Create a password to secure your settings.\n"
        "Password must be 4-16 characters long.");
}

static bool save_password_to_nvs(const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(PASSWORD_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(nvs_handle, PASSWORD_NVS_KEY, password);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error setting password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error committing NVS: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(s_tag, "Password successfully saved to NVS");
    return true;
}

static bool load_password_from_nvs(char *buffer, size_t buffer_size)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(PASSWORD_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS: %s", esp_err_to_name(err));
        return false;
    }
    size_t required_size = buffer_size;
    err = nvs_get_str(nvs_handle, PASSWORD_NVS_KEY, buffer, &required_size);
    nvs_close(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error getting password: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

static void show_message(const char *message, lv_color_t color)
{
    lv_label_set_text(define_password.message_label, message);
    lv_obj_set_style_text_color(define_password.message_label, color, 0);
}

static void clear_inputs(void)
{
    lv_textarea_set_text(define_password.password_textarea, "");
    lv_textarea_set_text(define_password.confirm_textarea, "");
    memset(define_password.password_buffer, 0, sizeof(define_password.password_buffer));
    memset(define_password.confirm_buffer, 0, sizeof(define_password.confirm_buffer));
    lv_label_set_text(define_password.message_label, "");
    lv_bar_set_value(define_password.strength_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(define_password.strength_label, "Strength: ");
    lv_obj_set_style_text_color(define_password.strength_label, lv_color_white(), 0);
}
