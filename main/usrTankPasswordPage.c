#include "usrTankPasswordPage.h"
#include "usrGeneralDefines.h"
#include "usrDefinePassword.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *s_tag = "usrTankPasswordPage";

static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Ana kurumsal renk
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Açık renk
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Koyu renk
static const lv_color_t color_background = LV_COLOR_MAKE(10, 10, 10);     // Arka plan

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
    tank_password_callback_t success_callback;
    tank_password_callback_t cancel_callback;
    bool is_initialized;
    bool is_visible;
    bool keyboard_visible;

    int attempt_count;
} usrTankPasswordPage_t;

static usrTankPasswordPage_t passwordPage = {0};

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

void usrTankPasswordPage_init(lv_obj_t *parent)
{
    if (passwordPage.is_initialized) {
        ESP_LOGW(s_tag, "Tank password page already initialized");
        return;
    }

    // Create modal background - tam ekran overlay (navbar'ın üstünde)
    passwordPage.modal_bg = lv_obj_create(lv_scr_act());  // Direkt aktif ekrana ekle
    lv_obj_set_size(passwordPage.modal_bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(passwordPage.modal_bg, 0, 0);
    lv_obj_set_style_bg_color(passwordPage.modal_bg, color_background, 0);
    lv_obj_set_style_bg_opa(passwordPage.modal_bg, LV_OPA_90, 0);  // %95 opaklık
    lv_obj_set_style_border_width(passwordPage.modal_bg, 0, 0);
    lv_obj_clear_flag(passwordPage.modal_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(passwordPage.modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(passwordPage.modal_bg, modal_bg_click_cb, LV_EVENT_CLICKED, NULL);
    
    // En üst katmanda olması için
    lv_obj_move_foreground(passwordPage.modal_bg);

    // Create main container - merkezi popup
    passwordPage.page_container = lv_obj_create(passwordPage.modal_bg);
    lv_obj_set_size(passwordPage.page_container, 400, 320);
    lv_obj_center(passwordPage.page_container);  // Tam merkeze yerleştir
    lv_obj_set_style_bg_color(passwordPage.page_container, color_background, 0);  // Siyah (10,10,10)
    lv_obj_set_style_border_color(passwordPage.page_container, color_primary, 0);  // Turkuaz (146,193,193)
    lv_obj_set_style_border_width(passwordPage.page_container, 2, 0);
    lv_obj_set_style_radius(passwordPage.page_container, 10, 0);
    lv_obj_set_style_pad_all(passwordPage.page_container, 20, 0);
    lv_obj_clear_flag(passwordPage.page_container, LV_OBJ_FLAG_SCROLLABLE);

    // Title label
    passwordPage.title_label = lv_label_create(passwordPage.page_container);
    lv_label_set_text(passwordPage.title_label, "ENTER PASSWORD");
    lv_obj_set_style_text_font(passwordPage.title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(passwordPage.title_label, color_primary, 0);  // Turkuaz (146,193,193)
    lv_obj_align(passwordPage.title_label, LV_ALIGN_TOP_MID, 0, 10);

    // Instruction label
    passwordPage.instruction_label = lv_label_create(passwordPage.page_container);
    lv_label_set_text(passwordPage.instruction_label, "Enter your password to access tank settings");
    lv_obj_set_style_text_font(passwordPage.instruction_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(passwordPage.instruction_label, color_secondary, 0);  // Beyaz (251,253,253)
    lv_obj_set_style_text_align(passwordPage.instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(passwordPage.instruction_label, LV_ALIGN_TOP_MID, 0, 45);

    // Password input textarea
    passwordPage.password_textarea = lv_textarea_create(passwordPage.page_container);
    lv_obj_set_size(passwordPage.password_textarea, 280, 45);
    lv_textarea_set_placeholder_text(passwordPage.password_textarea, "Enter password...");
    lv_textarea_set_password_mode(passwordPage.password_textarea, true);
    lv_textarea_set_max_length(passwordPage.password_textarea, PASSWORD_MAX_LENGTH);
    lv_obj_align(passwordPage.password_textarea, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_event_cb(passwordPage.password_textarea, textarea_event_cb, LV_EVENT_ALL, NULL);

    // Style textarea
    lv_obj_set_style_bg_color(passwordPage.password_textarea, color_dark, 0);  // Koyu gri (24,24,24)
    lv_obj_set_style_text_color(passwordPage.password_textarea, color_secondary, 0);  // Beyaz (251,253,253)
    lv_obj_set_style_border_color(passwordPage.password_textarea, color_primary, 0);  // Turkuaz (146,193,193)
    lv_obj_set_style_border_width(passwordPage.password_textarea, 2, 0);
    lv_obj_set_style_radius(passwordPage.password_textarea, 5, 0);

    // Message label
    passwordPage.message_label = lv_label_create(passwordPage.page_container);
    lv_label_set_text(passwordPage.message_label, "");
    lv_obj_set_style_text_font(passwordPage.message_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(passwordPage.message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(passwordPage.message_label, LV_ALIGN_CENTER, 0, 35);

    // Buttons container
    lv_obj_t *btn_container = lv_obj_create(passwordPage.page_container);
    lv_obj_set_size(btn_container, 280, 50);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);

    // Confirm button
    passwordPage.confirm_btn = lv_btn_create(btn_container);
    lv_obj_set_size(passwordPage.confirm_btn, 120, 45);
    lv_obj_set_pos(passwordPage.confirm_btn, 10, 0);
    lv_obj_set_style_bg_color(passwordPage.confirm_btn, color_primary, 0);  // Turkuaz (146,193,193)
    lv_obj_set_style_radius(passwordPage.confirm_btn, 5, 0);
    lv_obj_add_event_cb(passwordPage.confirm_btn, confirm_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *confirm_label = lv_label_create(passwordPage.confirm_btn);
    lv_label_set_text(confirm_label, LV_SYMBOL_OK " ENTER");
    lv_obj_set_style_text_font(confirm_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(confirm_label, color_dark, 0);  // Koyu yazı (24,24,24)
    lv_obj_center(confirm_label);

    // Cancel button
    passwordPage.cancel_btn = lv_btn_create(btn_container);
    lv_obj_set_size(passwordPage.cancel_btn, 120, 45);
    lv_obj_set_pos(passwordPage.cancel_btn, 150, 0);
    lv_obj_set_style_bg_color(passwordPage.cancel_btn, color_dark, 0);  // Koyu (24,24,24)
    lv_obj_set_style_border_color(passwordPage.cancel_btn, color_primary, 0);  // Turkuaz border (146,193,193)
    lv_obj_set_style_radius(passwordPage.cancel_btn, 5, 0);
    lv_obj_add_event_cb(passwordPage.cancel_btn, cancel_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *cancel_label = lv_label_create(passwordPage.cancel_btn);
    lv_label_set_text(cancel_label, LV_SYMBOL_CLOSE " CANCEL");
    lv_obj_set_style_text_color(cancel_label, color_secondary, 0);  // Beyaz yazı (251,253,253)
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_12, 0);
    lv_obj_center(cancel_label);

    // Create keyboard (initially hidden)
    passwordPage.keyboard = lv_keyboard_create(passwordPage.modal_bg);
    lv_obj_set_size(passwordPage.keyboard, LV_HOR_RES - 40, 160);
    lv_obj_align(passwordPage.keyboard, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(passwordPage.keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(passwordPage.keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Style keyboard
    lv_obj_set_style_bg_color(passwordPage.keyboard, color_dark, 0);  // Koyu (24,24,24)
    lv_obj_set_style_border_color(passwordPage.keyboard, color_primary, 0);  // Turkuaz border (146,193,193)
    lv_obj_set_style_border_width(passwordPage.keyboard, 2, 0);
    lv_obj_set_style_radius(passwordPage.keyboard, 8, 0);

    // Initialize state
    passwordPage.is_initialized = true;
    passwordPage.is_visible = false;
    passwordPage.keyboard_visible = false;
    passwordPage.attempt_count = 0;
    passwordPage.success_callback = NULL;
    passwordPage.cancel_callback = NULL;
    memset(passwordPage.password_buffer, 0, sizeof(passwordPage.password_buffer));

    // Hide by default
    lv_obj_add_flag(passwordPage.modal_bg, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(s_tag, "Tank password page initialized");
}

void usrTankPasswordPage_show(tank_password_callback_t success_callback, tank_password_callback_t cancel_callback)
{
    if (!passwordPage.is_initialized) {
        ESP_LOGW(s_tag, "Tank password page not initialized");
        return;
    }

    passwordPage.success_callback = success_callback;
    passwordPage.cancel_callback = cancel_callback;
    passwordPage.attempt_count = 0;

    // Clear previous input
    clear_password_input();
    show_message("", lv_color_white());

    // Show modal - en üstte göster
    lv_obj_clear_flag(passwordPage.modal_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(passwordPage.modal_bg);  // Kesinlikle en üstte olsun
    passwordPage.is_visible = true;

    ESP_LOGI(s_tag, "Tank password page shown on top layer");
}

void usrTankPasswordPage_hide(void)
{
    if (!passwordPage.is_initialized) return;

    lv_obj_add_flag(passwordPage.modal_bg, LV_OBJ_FLAG_HIDDEN);
    passwordPage.is_visible = false;
    hide_keyboard();
    clear_password_input();

    ESP_LOGI(s_tag, "Tank password page hidden");
}

void usrTankPasswordPage_destroy(void)
{
    if (!passwordPage.is_initialized) return;

    if (passwordPage.modal_bg) {
        lv_obj_del(passwordPage.modal_bg);
    }

    memset(&passwordPage, 0, sizeof(usrTankPasswordPage_t));

    ESP_LOGI(s_tag, "Tank password page destroyed");
}

bool usrTankPasswordPage_isVisible(void)
{
    return passwordPage.is_visible;
}

// Static functions

static void modal_bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *current_target = lv_event_get_current_target(e);
    
    // Modal background'a tıklandığında klavyeyi gizle
    if (target == current_target && target == passwordPage.modal_bg) {
        if (passwordPage.keyboard_visible) {
            hide_keyboard();
            lv_obj_clear_state(passwordPage.password_textarea, LV_STATE_FOCUSED);
            ESP_LOGI(s_tag, "Modal background clicked - keyboard hidden");
        }
    }
}

static void textarea_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        const char *text = lv_textarea_get_text(passwordPage.password_textarea);
        strncpy(passwordPage.password_buffer, text, PASSWORD_MAX_LENGTH);
        passwordPage.password_buffer[PASSWORD_MAX_LENGTH] = '\0';
        
        // Clear any previous error messages when user starts typing
        if (strlen(passwordPage.password_buffer) > 0) {
            show_message("", lv_color_white());
        }
    }
    else if (code == LV_EVENT_FOCUSED) {
        show_keyboard();
    }
}

static void confirm_button_cb(lv_event_t *e)
{
    if (strlen(passwordPage.password_buffer) == 0) {
        show_message("Please enter password", lv_color_make(255, 255, 0));
        return;
    }

    // Verify password using existing function
    if (usrDefinePassword_verifyPassword(passwordPage.password_buffer)) {
        // Password correct
        show_message("Access granted!", lv_color_make(0, 255, 0));
        hide_keyboard();

        if (passwordPage.success_callback) {
            passwordPage.success_callback();
        }

        ESP_LOGI(s_tag, "Password verified successfully");
        usrTankPasswordPage_hide();
    } else {
        // Password incorrect - deneme hakkı kontrolü kaldırıldı.
        show_message("Wrong password! Please try again.", lv_color_make(255, 0, 0));
        clear_password_input();

        ESP_LOGW(s_tag, "Password verification failed");
    }
}

static void cancel_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Cancel button pressed");
    
    hide_keyboard();
    
    if (passwordPage.cancel_callback) {
        passwordPage.cancel_callback();
    }
    
    usrTankPasswordPage_hide();
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY) {
        // Enter tuşuna basıldığında confirm butonunu tetikle
        lv_event_send(passwordPage.confirm_btn, LV_EVENT_CLICKED, NULL);
    }
    else if (code == LV_EVENT_CANCEL) {
        // ESC tuşuna basıldığında klavyeyi gizle
        hide_keyboard();
        lv_obj_clear_state(passwordPage.password_textarea, LV_STATE_FOCUSED);
    }
}

static void show_keyboard(void)
{
    if (!passwordPage.keyboard_visible) {
        lv_obj_clear_flag(passwordPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        passwordPage.keyboard_visible = true;
        lv_keyboard_set_textarea(passwordPage.keyboard, passwordPage.password_textarea);
        lv_obj_move_foreground(passwordPage.keyboard);
        ESP_LOGD(s_tag, "Keyboard shown");
    }
}

static void hide_keyboard(void)
{
    if (passwordPage.keyboard_visible) {
        lv_obj_add_flag(passwordPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        passwordPage.keyboard_visible = false;
        lv_keyboard_set_textarea(passwordPage.keyboard, NULL);
        ESP_LOGD(s_tag, "Keyboard hidden");
    }
}

static void show_message(const char *message, lv_color_t color)
{
    lv_label_set_text(passwordPage.message_label, message);
    lv_obj_set_style_text_color(passwordPage.message_label, color, 0);
}

static void clear_password_input(void)
{
    lv_textarea_set_text(passwordPage.password_textarea, "");
    memset(passwordPage.password_buffer, 0, sizeof(passwordPage.password_buffer));
}