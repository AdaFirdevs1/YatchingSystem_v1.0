#include "usrShuntPasswordPage.h"
#include "usrGeneralDefines.h"
#include "usrDefinePassword.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *s_tag = "usrShuntPasswordPage";


// Corporate color scheme - Battery sayfasıyla uyumlu
static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Primary corporate color
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Light background
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Dark text/borders
static const lv_color_t color_background = LV_COLOR_MAKE(10, 10, 10);     // Main background


typedef struct {
    lv_obj_t *modal_bg;
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *instruction_label;
    lv_obj_t *password_textarea;
    lv_obj_t *message_label;
    lv_obj_t *keyboard;
    lv_obj_t *confirm_btn;
    lv_obj_t *cancel_btn;
    
    char password_buffer[PASSWORD_MAX_LENGTH + 1];
    shunt_password_callback_t success_callback;
    shunt_password_callback_t cancel_callback;
    bool is_initialized;
    bool is_visible;
    bool keyboard_visible;

    int attempt_count;
} usrShuntPasswordPage_t;

static usrShuntPasswordPage_t shuntPasswordPage = {0};

// Function prototypes
static void modal_bg_click_cb(lv_event_t *e);
static void textarea_event_cb(lv_event_t *e);
static void confirm_button_cb(lv_event_t *e);
static void cancel_button_cb(lv_event_t *e);
static void keyboard_event_cb(lv_event_t *e);
static void show_keyboard(void);
static void hide_keyboard(void);
static void show_message(const char *message, lv_color_t color);
static void clear_password_input(void);

void usrShuntPasswordPage_init(lv_obj_t *parent)
{
    if (shuntPasswordPage.is_initialized) {
        ESP_LOGW(s_tag, "Shunt password page already initialized");
        return;
    }

    // Create modal background - tam ekran overlay (navbar'ın üstünde)
    shuntPasswordPage.modal_bg = lv_obj_create(lv_scr_act());  // Direkt aktif ekrana ekle
    lv_obj_set_size(shuntPasswordPage.modal_bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(shuntPasswordPage.modal_bg, 0, 0);
    lv_obj_set_style_bg_color(shuntPasswordPage.modal_bg, lv_color_make(0, 0, 0), 0);  // Tamamen siyah
    lv_obj_set_style_bg_opa(shuntPasswordPage.modal_bg, LV_OPA_90, 0);  // %90 opaklık
    lv_obj_set_style_border_width(shuntPasswordPage.modal_bg, 0, 0);
    lv_obj_clear_flag(shuntPasswordPage.modal_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shuntPasswordPage.modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(shuntPasswordPage.modal_bg, modal_bg_click_cb, LV_EVENT_CLICKED, NULL);
    
    // En üst katmanda olması için
    lv_obj_move_foreground(shuntPasswordPage.modal_bg);

    // Create main container - merkezi popup
    shuntPasswordPage.page_container = lv_obj_create(shuntPasswordPage.modal_bg);
    lv_obj_set_size(shuntPasswordPage.page_container, 400, 320);
    lv_obj_center(shuntPasswordPage.page_container);  // Tam merkeze yerleştir
    lv_obj_set_style_bg_color(shuntPasswordPage.page_container, color_background, 0);
    lv_obj_set_style_border_color(shuntPasswordPage.page_container, color_primary, 0);
    lv_obj_set_style_border_width(shuntPasswordPage.page_container, 2, 0);
    lv_obj_set_style_radius(shuntPasswordPage.page_container, 10, 0);
    lv_obj_set_style_pad_all(shuntPasswordPage.page_container, 20, 0);
    lv_obj_clear_flag(shuntPasswordPage.page_container, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    shuntPasswordPage.title_label = lv_label_create(shuntPasswordPage.page_container);
    lv_label_set_text(shuntPasswordPage.title_label, "ENTER PASSWORD");
    lv_obj_set_style_text_font(shuntPasswordPage.title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(shuntPasswordPage.title_label, color_primary, 0);
    lv_obj_align(shuntPasswordPage.title_label, LV_ALIGN_TOP_MID, 0, 10);

    // Instruction label
    shuntPasswordPage.instruction_label = lv_label_create(shuntPasswordPage.page_container);
    lv_label_set_text(shuntPasswordPage.instruction_label, "Enter your password to access shunt settings");
    lv_obj_set_style_text_font(shuntPasswordPage.instruction_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(shuntPasswordPage.instruction_label, color_secondary, 0);
    lv_obj_set_style_text_align(shuntPasswordPage.instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(shuntPasswordPage.instruction_label, LV_ALIGN_TOP_MID, 0, 45);

    // Password input textarea
    shuntPasswordPage.password_textarea = lv_textarea_create(shuntPasswordPage.page_container);
    lv_obj_set_size(shuntPasswordPage.password_textarea, 280, 45);
    lv_textarea_set_placeholder_text(shuntPasswordPage.password_textarea, "Enter password...");
    lv_textarea_set_password_mode(shuntPasswordPage.password_textarea, true);
    lv_textarea_set_max_length(shuntPasswordPage.password_textarea, PASSWORD_MAX_LENGTH);
    lv_obj_align(shuntPasswordPage.password_textarea, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(shuntPasswordPage.password_textarea, textarea_event_cb, LV_EVENT_ALL, NULL);

    // Style textarea
    lv_obj_set_style_bg_color(shuntPasswordPage.password_textarea, color_dark, 0);  // Koyu gri (24,24,24)
    lv_obj_set_style_text_color(shuntPasswordPage.password_textarea, color_secondary, 0);  // Beyaz (251,253,253)
    lv_obj_set_style_border_color(shuntPasswordPage.password_textarea, color_primary, 0);  // Turkuaz (146,193,193)
    lv_obj_set_style_border_width(shuntPasswordPage.password_textarea, 2, 0);
    lv_obj_set_style_radius(shuntPasswordPage.password_textarea, 5, 0);

    // Message label
    shuntPasswordPage.message_label = lv_label_create(shuntPasswordPage.page_container);
    lv_label_set_text(shuntPasswordPage.message_label, "");
    lv_obj_set_style_text_font(shuntPasswordPage.message_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(shuntPasswordPage.message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(shuntPasswordPage.message_label, LV_ALIGN_CENTER, 0, 35);

    // Buttons container
    lv_obj_t *btn_container = lv_obj_create(shuntPasswordPage.page_container);
    lv_obj_set_size(btn_container, 280, 50);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);

    // Confirm button
    shuntPasswordPage.confirm_btn = lv_btn_create(btn_container);
    lv_obj_set_size(shuntPasswordPage.confirm_btn, 120, 45);
    lv_obj_set_pos(shuntPasswordPage.confirm_btn, 10, 0);
    lv_obj_set_style_bg_color(shuntPasswordPage.confirm_btn, color_primary, 0);
    lv_obj_set_style_radius(shuntPasswordPage.confirm_btn, 5, 0);
    lv_obj_add_event_cb(shuntPasswordPage.confirm_btn, confirm_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *confirm_label = lv_label_create(shuntPasswordPage.confirm_btn);
    lv_label_set_text(confirm_label, LV_SYMBOL_OK " ENTER");
    lv_obj_set_style_text_font(confirm_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(confirm_label, color_dark, 0);
    lv_obj_center(confirm_label);

    // Cancel button
    shuntPasswordPage.cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(shuntPasswordPage.cancel_btn, 120, 45);
    lv_obj_set_pos(shuntPasswordPage.cancel_btn, 150, 0);
    lv_obj_set_style_bg_color(shuntPasswordPage.cancel_btn, color_dark, 0);
    lv_obj_set_style_radius(shuntPasswordPage.cancel_btn, 5, 0);
    lv_obj_add_event_cb(shuntPasswordPage.cancel_btn, cancel_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_border_color(shuntPasswordPage.cancel_btn, color_primary, 0);  // Turkuaz border (146,193,193)
    
    lv_obj_t *cancel_label = lv_label_create(shuntPasswordPage.cancel_btn);
    lv_label_set_text(cancel_label, LV_SYMBOL_CLOSE " CANCEL");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cancel_label, color_secondary, 0);
    lv_obj_center(cancel_label);

    // Create keyboard (initially hidden)
    shuntPasswordPage.keyboard = lv_keyboard_create(shuntPasswordPage.modal_bg);
    lv_obj_set_size(shuntPasswordPage.keyboard, LV_HOR_RES - 40, 160);
    lv_obj_align(shuntPasswordPage.keyboard, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(shuntPasswordPage.keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(shuntPasswordPage.keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Style keyboard
    lv_obj_set_style_bg_color(shuntPasswordPage.keyboard, color_dark, 0);  // Koyu (24,24,24)
    lv_obj_set_style_border_color(shuntPasswordPage.keyboard, color_primary, 0);  // Turkuaz border (146,193,193)
    lv_obj_set_style_border_width(shuntPasswordPage.keyboard, 2, 0);
    lv_obj_set_style_radius(shuntPasswordPage.keyboard, 8, 0);

    // Initialize state
    shuntPasswordPage.is_initialized = true;
    shuntPasswordPage.is_visible = false;
    shuntPasswordPage.keyboard_visible = false;
    shuntPasswordPage.attempt_count = 0;
    shuntPasswordPage.success_callback = NULL;
    shuntPasswordPage.cancel_callback = NULL;
    memset(shuntPasswordPage.password_buffer, 0, sizeof(shuntPasswordPage.password_buffer));

    // Hide by default
    lv_obj_add_flag(shuntPasswordPage.modal_bg, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(s_tag, "Shunt password page initialized");
}

void usrShuntPasswordPage_show(shunt_password_callback_t success_callback, shunt_password_callback_t cancel_callback)
{
    if (!shuntPasswordPage.is_initialized) {
        ESP_LOGW(s_tag, "Shunt password page not initialized");
        return;
    }

    shuntPasswordPage.success_callback = success_callback;
    shuntPasswordPage.cancel_callback = cancel_callback;
    shuntPasswordPage.attempt_count = 0;

    // Clear previous input
    clear_password_input();
    show_message("", lv_color_white());

    // Show modal - en üstte göster
    lv_obj_clear_flag(shuntPasswordPage.modal_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(shuntPasswordPage.modal_bg);  // Kesinlikle en üstte olsun
    shuntPasswordPage.is_visible = true;

    ESP_LOGI(s_tag, "Shunt password page shown on top layer");
}

void usrShuntPasswordPage_hide(void)
{
    if (!shuntPasswordPage.is_initialized) return;

    lv_obj_add_flag(shuntPasswordPage.modal_bg, LV_OBJ_FLAG_HIDDEN);
    shuntPasswordPage.is_visible = false;
    hide_keyboard();
    clear_password_input();

    ESP_LOGI(s_tag, "Shunt password page hidden");
}

void usrShuntPasswordPage_destroy(void)
{
    if (!shuntPasswordPage.is_initialized) return;

    if (shuntPasswordPage.modal_bg) {
        lv_obj_del(shuntPasswordPage.modal_bg);
    }

    memset(&shuntPasswordPage, 0, sizeof(usrShuntPasswordPage_t));

    ESP_LOGI(s_tag, "Shunt password page destroyed");
}

bool usrShuntPasswordPage_isVisible(void)
{
    return shuntPasswordPage.is_visible;
}

// Static functions

static void modal_bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *current_target = lv_event_get_current_target(e);
    
    // Modal background'a tıklandığında klavyeyi gizle
    if (target == current_target && target == shuntPasswordPage.modal_bg) {
        if (shuntPasswordPage.keyboard_visible) {
            hide_keyboard();
            lv_obj_clear_state(shuntPasswordPage.password_textarea, LV_STATE_FOCUSED);
            ESP_LOGI(s_tag, "Modal background clicked - keyboard hidden");
        }
    }
}

static void textarea_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        const char *text = lv_textarea_get_text(shuntPasswordPage.password_textarea);
        strncpy(shuntPasswordPage.password_buffer, text, PASSWORD_MAX_LENGTH);
        shuntPasswordPage.password_buffer[PASSWORD_MAX_LENGTH] = '\0';
        
        // Clear any previous error messages when user starts typing
        if (strlen(shuntPasswordPage.password_buffer) > 0) {
            show_message("", lv_color_white());
        }
    }
    else if (code == LV_EVENT_FOCUSED) {
        show_keyboard();
    }
}

static void confirm_button_cb(lv_event_t *e)
{
    if (strlen(shuntPasswordPage.password_buffer) == 0) {
        show_message("Please enter password", lv_color_make(255, 255, 0));
        return;
    }

    // Verify password using existing function
    if (usrDefinePassword_verifyPassword(shuntPasswordPage.password_buffer)) {
        // Password correct
        show_message("Access granted to shunt settings!", lv_color_make(0, 255, 0));
        hide_keyboard();

        if (shuntPasswordPage.success_callback) {
            shuntPasswordPage.success_callback();
        }

        ESP_LOGI(s_tag, "Shunt settings password verified successfully");
        usrShuntPasswordPage_hide();
    } else {
        // Password incorrect
        show_message("Wrong password! Please try again.", lv_color_make(255, 0, 0));
        clear_password_input();

        ESP_LOGW(s_tag, "Shunt settings password verification failed");
    }
}

static void cancel_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Cancel button pressed");
    
    hide_keyboard();
    
    if (shuntPasswordPage.cancel_callback) {
        shuntPasswordPage.cancel_callback();
    }
    
    usrShuntPasswordPage_hide();
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY) {
        // Enter tuşuna basıldığında confirm butonunu tetikle
        lv_event_send(shuntPasswordPage.confirm_btn, LV_EVENT_CLICKED, NULL);
    }
    else if (code == LV_EVENT_CANCEL) {
        // ESC tuşuna basıldığında klavyeyi gizle
        hide_keyboard();
        lv_obj_clear_state(shuntPasswordPage.password_textarea, LV_STATE_FOCUSED);
    }
}

static void show_keyboard(void)
{
    if (!shuntPasswordPage.keyboard_visible) {
        lv_obj_clear_flag(shuntPasswordPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        shuntPasswordPage.keyboard_visible = true;
        lv_keyboard_set_textarea(shuntPasswordPage.keyboard, shuntPasswordPage.password_textarea);
        lv_obj_move_foreground(shuntPasswordPage.keyboard);
        ESP_LOGD(s_tag, "Keyboard shown");
    }
}

static void hide_keyboard(void)
{
    if (shuntPasswordPage.keyboard_visible) {
        lv_obj_add_flag(shuntPasswordPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        shuntPasswordPage.keyboard_visible = false;
        lv_keyboard_set_textarea(shuntPasswordPage.keyboard, NULL);
        ESP_LOGD(s_tag, "Keyboard hidden");
    }
}

static void show_message(const char *message, lv_color_t color)
{
    lv_label_set_text(shuntPasswordPage.message_label, message);
    lv_obj_set_style_text_color(shuntPasswordPage.message_label, color, 0);
}

static void clear_password_input(void)
{
    lv_textarea_set_text(shuntPasswordPage.password_textarea, "");
    memset(shuntPasswordPage.password_buffer, 0, sizeof(shuntPasswordPage.password_buffer));
}