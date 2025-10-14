#include "usrButtonPage.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include "usrButtonSettingsPage.h"
#include "usrButtonPasswordPage.h"
#include "usrMainPage.h"

static const char *s_tag = "usrButtonPage";
static usrButtonPage_t buttonPage = {0};

// Global slider position storage
static uint32_t sliderPos[6] = {0, 0, 0, 0, 0, 0};
static uint32_t lastSliderPos[6] = {0, 0, 0, 0, 0, 0}; // Store last non-zero positions

// Button names
static const char *button_names[6] = {
    "PWM-1",
    "PWM-2",
    "PWM-3",
    "PWM-4",
    "PWM-5",
    "PWM-6"};

// CAN ID definitions
#define CAN_ID_BUTTON_BASE 0x400 // 0x400-0x405 button states
#define CAN_ID_SLIDER_BASE 0x406 // 0x406-0x40B slider values
#define NVS_NAMESPACE_BUTTON_STATES "btn_states"
#define NVS_KEY_BUTTON_STATES "states"
#define NVS_KEY_SLIDER_VALUES "sliders"

// Corporate color scheme
static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Primary corporate color
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Light background
static const lv_color_t color_accent = LV_COLOR_MAKE(216, 216, 216);      // Accent gray
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Dark text/borders
static const lv_color_t color_inactive = LV_COLOR_MAKE(100, 100, 100);    // Inactive state

// Arc/Button properties
#define ARC_SIZE 130
#define ARC_WIDTH 12

// Forward declarations
static void settings_button_cb(lv_event_t *e);
static void settings_page_back_callback(void);
static void password_success_callback(void);
static void password_cancel_callback(void);
static void arc_event_cb(lv_event_t *e);
static void center_button_event_cb(lv_event_t *e);

// Settings button callback
static void settings_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Settings button pressed - requesting password");
    usrButtonPasswordPage_show(password_success_callback, password_cancel_callback);
}

// Password success callback
static void password_success_callback(void)
{
    ESP_LOGI(s_tag, "Password verified - opening settings page");
    usrButtonPage_hide();
    usrButtonSettingsPage_show();
}

// Password cancel callback
static void password_cancel_callback(void)
{
    ESP_LOGI(s_tag, "Password entry cancelled - staying on button page");
}

// Settings page back callback
static void settings_page_back_callback(void)
{
    ESP_LOGI(s_tag, "=== BACK CALLBACK - REFRESHING FROM NVS ===");
    
    usrButtonSettingsPage_hide();
    usrButtonPage_show();
    
    // Load updated data from NVS
    char updated_button_names[6][11];
    
    esp_err_t names_ret = load_button_names_from_nvs(updated_button_names);
    
    if (names_ret == ESP_OK) {
        // Update button labels
        for (int i = 0; i < 6; i++) {
            if (buttonPage.center_button_labels[i] != NULL) {
                lv_label_set_text(buttonPage.center_button_labels[i], updated_button_names[i]);
            }
            
            // Update value labels
            if (buttonPage.value_labels[i] != NULL) {
                char label_text[32];
                snprintf(label_text, sizeof(label_text), "%ld", 
                         sliderPos[i]);
                lv_label_set_text(buttonPage.value_labels[i], label_text);
            }
        }
        ESP_LOGI(s_tag, "Button names updated from NVS");
    }
    
    
}

// Arc event callback for PWM adjustment
static void arc_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *arc = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        // Find which arc was changed
        int channel = -1;
        for (int i = 0; i < 6; i++)
        {
            if (buttonPage.arc_meters[i] == arc)
            {
                channel = i;
                break;
            }
        }

        if (channel >= 0 && buttonPage.button_states[channel])
        {
            // Get arc value (0-1000)
            int32_t value = lv_arc_get_value(arc);

            // Store in global variable
            sliderPos[channel] = (uint32_t)value;
            lastSliderPos[channel] = (uint32_t)value; 
            buttonPage.slider_values[channel] = value;

            // Update value label
            const char* button_name = lv_label_get_text(buttonPage.center_button_labels[channel]);
            char value_text[32];
            snprintf(value_text, sizeof(value_text), "%ld",value);
            lv_label_set_text(buttonPage.value_labels[channel], value_text);

            // Send CAN message
            uint32_t can_id = CAN_ID_SLIDER_BASE + channel;
            esp_err_t result = sendCanHeader(can_id, (uint32_t)value);
            if (result == ESP_OK)
            {
                ESP_LOGI(s_tag, "Arc %d value sent: %ld (ID: 0x%lX)",
                         channel + 1, value, can_id);
            }

        }
    }
    save_slider_values_to_nvs();
}

// Center button event callback
static void center_button_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        // Find which button was pressed
        for (int i = 0; i < 6; i++)
        {
            if (buttonPage.center_buttons[i] == btn)
            {
                // Call appropriate handler
                switch (i)
                {
                case 0: usrButtonPage_button1_handler(); break;
                case 1: usrButtonPage_button2_handler(); break;
                case 2: usrButtonPage_button3_handler(); break;
                case 3: usrButtonPage_button4_handler(); break;
                case 4: usrButtonPage_button5_handler(); break;
                case 5: usrButtonPage_button6_handler(); break;
                }

                // Update status
                const char* current_name = lv_label_get_text(buttonPage.center_button_labels[i]);
                char status_text[64];
                snprintf(status_text, sizeof(status_text), "%s %s",
                         current_name, buttonPage.button_states[i] ? "ON" : "OFF");
                usrButtonPage_setStatus(status_text);

                ESP_LOGI(s_tag, "Button %d (%s) pressed", i + 1, current_name);
                break;
            }
        }
    }
    else if (code == LV_EVENT_PRESSED)
    {
        // Add glow effect when pressed
        lv_obj_set_style_shadow_width(btn, 15, 0);
        lv_obj_set_style_shadow_color(btn, color_primary, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_80, 0);
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST)
    {
        // Remove glow effect when released
        lv_obj_set_style_shadow_width(btn, 8, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    }
}

void usrButtonPage_init(lv_obj_t *parent)
{
    if (buttonPage.page_container != NULL)
    {
        ESP_LOGW(s_tag, "Button page already initialized");
        return;
    }

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create main container
    buttonPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(buttonPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(buttonPage.page_container, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_border_width(buttonPage.page_container, 0, 0);
    lv_obj_clear_flag(buttonPage.page_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create title container
    lv_obj_t *title_container = lv_obj_create(buttonPage.page_container);
    lv_obj_set_size(title_container, LV_HOR_RES, 50);
    lv_obj_set_pos(title_container, 0, 0);
    lv_obj_set_style_bg_color(title_container, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create title
    buttonPage.title_label = lv_label_create(title_container);
    lv_label_set_text(buttonPage.title_label, "PWM CONTROL PANEL");
    lv_obj_set_style_text_font(buttonPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(buttonPage.title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(buttonPage.title_label, LV_ALIGN_CENTER, -60, -10);

    // Settings button
    buttonPage.settings_button = lv_btn_create(title_container);
    lv_obj_set_size(buttonPage.settings_button, 40, 40);
    lv_obj_set_pos(buttonPage.settings_button, LV_HOR_RES - 170, -20);
    lv_obj_set_style_bg_color(buttonPage.settings_button, color_primary, 0);
    lv_obj_set_style_radius(buttonPage.settings_button, 5, 0);
    lv_obj_add_event_cb(buttonPage.settings_button, settings_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *settings_icon = lv_label_create(buttonPage.settings_button);
    lv_label_set_text(settings_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(settings_icon, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(settings_icon, color_dark, 0);
    lv_obj_center(settings_icon);

    // Initialize password and settings pages
    usrButtonPasswordPage_init(parent);
    usrButtonSettingsPage_init(parent);
    usrButtonSettingsPage_setBackCallback(settings_page_back_callback);

    // Create circular controls in 3x2 layout
    int spacing_x = 80;
    int spacing_y = 10;
    int total_width = (3 * ARC_SIZE) + (2 * spacing_x);
    int start_x = (800 - total_width) / 2 - 70;
    int start_y = 50;

    for (int i = 0; i < 6; i++)
    {
        // Calculate position for 3x2 grid
        int col = i % 3;
        int row = i / 3;
        int x = start_x + col * (ARC_SIZE + spacing_x);
        int y = start_y + row * (ARC_SIZE + spacing_y + 20);

        // Create arc meter (PWM control)
        buttonPage.arc_meters[i] = lv_arc_create(buttonPage.page_container);
        lv_obj_set_size(buttonPage.arc_meters[i], ARC_SIZE + 10, ARC_SIZE + 10);
        lv_obj_set_pos(buttonPage.arc_meters[i], x, y);

        // Configure arc
        lv_arc_set_range(buttonPage.arc_meters[i], 0, 1000);
        lv_arc_set_value(buttonPage.arc_meters[i], 0);
        lv_arc_set_bg_angles(buttonPage.arc_meters[i], 0, 360);

        // Style the arc
        lv_obj_set_style_arc_color(buttonPage.arc_meters[i], color_accent, LV_PART_MAIN);
        lv_obj_set_style_arc_color(buttonPage.arc_meters[i], color_inactive, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(buttonPage.arc_meters[i], ARC_WIDTH, LV_PART_MAIN);
        lv_obj_set_style_arc_width(buttonPage.arc_meters[i], ARC_WIDTH, LV_PART_INDICATOR);

        // Style knob (make it more prominent)
        lv_obj_set_style_bg_color(buttonPage.arc_meters[i], color_primary, LV_PART_KNOB);
        //lv_obj_set_style_border_color(buttonPage.arc_meters[i], lv_palette_main(LV_PALETTE_CYAN), LV_PART_KNOB);
        //lv_obj_set_style_border_width(buttonPage.arc_meters[i], 2, LV_PART_KNOB);

        // Initially disable arc (until button is activated)
        lv_obj_add_state(buttonPage.arc_meters[i], LV_STATE_DISABLED);
        lv_obj_add_event_cb(buttonPage.arc_meters[i], arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

        // Create center button (clickable area for on/off)
        buttonPage.center_buttons[i] = lv_btn_create(buttonPage.page_container);
        lv_obj_set_size(buttonPage.center_buttons[i], ARC_SIZE - 30, ARC_SIZE - 30);
        lv_obj_align_to(buttonPage.center_buttons[i], buttonPage.arc_meters[i], LV_ALIGN_CENTER, 0, 0);
        
        // Style center button
        lv_obj_set_style_bg_color(buttonPage.center_buttons[i], color_secondary, 0);
        lv_obj_set_style_bg_color(buttonPage.center_buttons[i], color_accent, LV_STATE_PRESSED);
        //lv_obj_set_style_border_color(buttonPage.center_buttons[i], lv_palette_main(LV_PALETTE_CYAN), 0);
        lv_obj_set_style_border_width(buttonPage.center_buttons[i], 2, 0);
        lv_obj_set_style_radius(buttonPage.center_buttons[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_shadow_width(buttonPage.center_buttons[i], 8, 0);
        lv_obj_set_style_shadow_color(buttonPage.center_buttons[i], color_dark, 0);
        lv_obj_set_style_shadow_opa(buttonPage.center_buttons[i], LV_OPA_30, 0);

        lv_obj_add_event_cb(buttonPage.center_buttons[i], center_button_event_cb, LV_EVENT_ALL, NULL);

        // Center button label (button name)
        buttonPage.center_button_labels[i] = lv_label_create(buttonPage.center_buttons[i]);
        lv_label_set_text(buttonPage.center_button_labels[i], button_names[i]);
        lv_obj_set_style_text_font(buttonPage.center_button_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(buttonPage.center_button_labels[i], color_dark, 0);
        lv_obj_set_style_text_align(buttonPage.center_button_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(buttonPage.center_button_labels[i]);

        // Value label (below arc)
        buttonPage.value_labels[i] = lv_label_create(buttonPage.page_container);
        lv_label_set_text(buttonPage.value_labels[i], "OFF");
        lv_obj_set_style_text_font(buttonPage.value_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(buttonPage.value_labels[i], color_dark, 0);
        lv_obj_set_style_text_align(buttonPage.value_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align_to(buttonPage.value_labels[i], buttonPage.center_button_labels[i], LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

        // Initialize states
        buttonPage.button_states[i] = false;
        buttonPage.slider_values[i] = 0;
    }

    // Create status label
    buttonPage.status_label = lv_label_create(buttonPage.page_container);
    lv_label_set_text(buttonPage.status_label, "PWM Control Ready - Click center buttons to activate");
    lv_obj_set_style_text_font(buttonPage.status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(buttonPage.status_label, color_dark, 0);
    lv_obj_align(buttonPage.status_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Load button names from settings
    char saved_button_names[6][11];
    usrButtonSettingsPage_loadButtonNames(saved_button_names);
    
    for (int i = 0; i < 6; i++) {
        lv_label_set_text(buttonPage.center_button_labels[i], saved_button_names[i]);
    }

    // Load saved states from NVS
    load_button_states_from_nvs();
    load_slider_values_from_nvs();

    // Apply loaded states to UI
    for (int i = 0; i < 6; i++) {
        if (buttonPage.button_states[i]) {
            // Enable arc and update colors
            lv_obj_clear_state(buttonPage.arc_meters[i], LV_STATE_DISABLED);
            lv_obj_set_style_arc_color(buttonPage.arc_meters[i], color_primary, LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(buttonPage.center_buttons[i], color_primary, 0);
            lv_obj_set_style_text_color(buttonPage.center_button_labels[i], color_secondary, 0);
            
            // Set arc value
            lv_arc_set_value(buttonPage.arc_meters[i], sliderPos[i]);
            
            // Update value label
            const char* current_name = lv_label_get_text(buttonPage.center_button_labels[i]);
            char label_text[32];
            snprintf(label_text, sizeof(label_text), "%ld", sliderPos[i]);
            lv_label_set_text(buttonPage.value_labels[i], label_text);
            
            // Send CAN messages on startup
            sendCanHeader(CAN_ID_BUTTON_BASE + i, 1);
            if (sliderPos[i] > 0) {
                sendCanHeader(CAN_ID_SLIDER_BASE + i, sliderPos[i]);
            }
        }
    }

    ESP_LOGI(s_tag, "Circular button page initialized with corporate colors");

    // Initially hide the page
    usrButtonPage_hide();
}


void usrButtonPage_show(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_clear_flag(buttonPage.page_container, LV_OBJ_FLAG_HIDDEN);
        buttonPage.is_active = true;
        
        // Load current states from NVS
        load_button_states_from_nvs();
        load_slider_values_from_nvs();
        
        // Update UI based on current states
        for (int i = 0; i < 6; i++) {
            if (buttonPage.button_states[i]) {
                lv_obj_clear_state(buttonPage.arc_meters[i], LV_STATE_DISABLED);
                lv_obj_set_style_arc_color(buttonPage.arc_meters[i], color_secondary, LV_PART_MAIN); // Ana yay secondary renkte
                lv_obj_set_style_arc_color(buttonPage.arc_meters[i], color_primary, LV_PART_INDICATOR);
                lv_obj_set_style_bg_color(buttonPage.center_buttons[i], color_primary, 0);
                lv_obj_set_style_text_color(buttonPage.center_button_labels[i], lv_color_white(), 0); // Yazıları beyaz yap
    
                
                // Aktif buton gölge efektleri
                lv_obj_set_style_shadow_width(buttonPage.center_buttons[i], 15, 0);
                lv_obj_set_style_shadow_color(buttonPage.center_buttons[i], color_primary, 0);
                lv_obj_set_style_shadow_opa(buttonPage.center_buttons[i], LV_OPA_60, 0);
                lv_obj_set_style_shadow_spread(buttonPage.center_buttons[i], 3, 0);
                
                // Arc gölge efekti
                lv_obj_set_style_shadow_width(buttonPage.arc_meters[i], 15, 0);
                lv_obj_set_style_shadow_color(buttonPage.arc_meters[i], color_primary, 0);
                lv_obj_set_style_shadow_opa(buttonPage.arc_meters[i], LV_OPA_60, 0);
                lv_obj_set_style_radius(buttonPage.arc_meters[i], LV_RADIUS_CIRCLE, 0);
                
                lv_arc_set_value(buttonPage.arc_meters[i], sliderPos[i]);
                
                const char* current_name = lv_label_get_text(buttonPage.center_button_labels[i]);
                char label_text[32];
                snprintf(label_text, sizeof(label_text), "%ld", sliderPos[i]);
                lv_label_set_text(buttonPage.value_labels[i], label_text);
            } else {
                lv_obj_add_state(buttonPage.arc_meters[i], LV_STATE_DISABLED);
    
                // Arc'ın hem main hem indicator kısmını inactive renkte yap
                lv_obj_set_style_arc_color(buttonPage.arc_meters[i], color_inactive, LV_PART_MAIN);
                lv_obj_set_style_arc_color(buttonPage.arc_meters[i], color_inactive, LV_PART_INDICATOR);
                
                lv_obj_set_style_bg_color(buttonPage.center_buttons[i], color_inactive, 0);
                lv_obj_set_style_text_color(buttonPage.center_button_labels[i], color_secondary, 0);
                
                // Knob'ı gizle - İLK YÜKLEME İÇİN EKLENDİ
                lv_obj_set_style_bg_opa(buttonPage.arc_meters[i], LV_OPA_TRANSP, LV_PART_KNOB);
                lv_obj_set_style_border_opa(buttonPage.arc_meters[i], LV_OPA_TRANSP, LV_PART_KNOB);
                
                // Pasif buton minimal gölge
                lv_obj_set_style_shadow_width(buttonPage.center_buttons[i], 5, 0);
                lv_obj_set_style_shadow_color(buttonPage.center_buttons[i], color_dark, 0);
                lv_obj_set_style_shadow_opa(buttonPage.center_buttons[i], LV_OPA_20, 0);
                lv_obj_set_style_shadow_spread(buttonPage.center_buttons[i], 0, 0);
                
                // Arc gölgesini kaldır
                lv_obj_set_style_shadow_width(buttonPage.arc_meters[i], 0, 0);
                lv_obj_set_style_shadow_opa(buttonPage.arc_meters[i], LV_OPA_TRANSP, 0);
                
                char label_text[32];
                snprintf(label_text, sizeof(label_text), "OFF");
                lv_label_set_text(buttonPage.value_labels[i], label_text);
            }
        }
        
        ESP_LOGI(s_tag, "Circular button page shown with restored states");
    }
}

void usrButtonPage_hide(void)
{
    if (buttonPage.page_container != NULL)
    {
        lv_obj_add_flag(buttonPage.page_container, LV_OBJ_FLAG_HIDDEN);
        buttonPage.is_active = false;
        ESP_LOGI(s_tag, "Circular button page hidden");
    }
}

void usrButtonPage_destroy(void)
{
    if (buttonPage.page_container != NULL)
    {
        usrButtonPasswordPage_destroy();
        usrButtonSettingsPage_destroy();
        
        lv_obj_del(buttonPage.page_container);
        memset(&buttonPage, 0, sizeof(usrButtonPage_t));
        ESP_LOGI(s_tag, "Circular button page destroyed");
    }
}

void usrButtonPage_setStatus(const char *status)
{
    if (buttonPage.status_label != NULL && status != NULL)
    {
        lv_label_set_text(buttonPage.status_label, status);
    }
}

bool usrButtonPage_isActive(void)
{
    return buttonPage.is_active;
}


// Generic button handler function
static void handle_button_toggle(int button_index)
{
    if (button_index < 0 || button_index >= 6) return;
    
    ESP_LOGI(s_tag, "Button-%d handler called", button_index + 1);

    // Toggle button state
    buttonPage.button_states[button_index] = !buttonPage.button_states[button_index];

    if (buttonPage.button_states[button_index])
    {
    
        // Active state
        lv_obj_clear_state(buttonPage.arc_meters[button_index], LV_STATE_DISABLED);
        lv_obj_set_style_arc_color(buttonPage.arc_meters[button_index], color_secondary, LV_PART_MAIN); // Ana yay secondary renkte
        lv_obj_set_style_arc_color(buttonPage.arc_meters[button_index], color_primary, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(buttonPage.center_buttons[button_index], color_primary, 0);
        lv_obj_set_style_text_color(buttonPage.center_button_labels[button_index], lv_color_white(), 0); // Yazıları beyaz yap

        // Knob'ı tekrar görünür yap
        lv_obj_set_style_bg_opa(buttonPage.arc_meters[button_index], LV_OPA_COVER, LV_PART_KNOB);
        lv_obj_set_style_border_opa(buttonPage.arc_meters[button_index], LV_OPA_COVER, LV_PART_KNOB);


        // Mevcut slider değerini koru
        lv_arc_set_value(buttonPage.arc_meters[button_index], sliderPos[button_index]);

        char label_text[32];
        snprintf(label_text, sizeof(label_text), "%ld", sliderPos[button_index]);
        lv_label_set_text(buttonPage.value_labels[button_index], label_text);

        // Sadece buton aktifleştiğinde CAN mesajı gönder
        if (sliderPos[button_index] > 0) {
            sendCanHeader(CAN_ID_SLIDER_BASE + button_index, sliderPos[button_index]);
        }
    }
    else
    {
        // Inactive state - Slider değerini koruyarak sadece UI'ı deaktive et
        lv_obj_add_state(buttonPage.arc_meters[button_index], LV_STATE_DISABLED);
        
        // Arc'ın hem main hem indicator kısmını inactive renkte yap
        lv_obj_set_style_arc_color(buttonPage.arc_meters[button_index], color_inactive, LV_PART_MAIN);
        lv_obj_set_style_arc_color(buttonPage.arc_meters[button_index], color_inactive, LV_PART_INDICATOR);
        
        lv_obj_set_style_bg_color(buttonPage.center_buttons[button_index], color_inactive, 0);
        lv_obj_set_style_text_color(buttonPage.center_button_labels[button_index], color_secondary, 0);

        // Knob'ı gizle
        lv_obj_set_style_bg_opa(buttonPage.arc_meters[button_index], LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_border_opa(buttonPage.arc_meters[button_index], LV_OPA_TRANSP, LV_PART_KNOB);

        // Slider değerini koruyarak sadece label'ı OFF yap
        lv_label_set_text(buttonPage.value_labels[button_index], "OFF");

        // Sadece kapatma sinyali gönder, slider değerini sıfırlama
        sendCanHeader(CAN_ID_SLIDER_BASE + button_index, 0);
    }

    // Send button state via CAN
    uint32_t button_value = buttonPage.button_states[button_index] ? 1 : 0;
    esp_err_t err = sendCanHeader(CAN_ID_BUTTON_BASE + button_index, button_value);
    if (err != ESP_OK)
    {
        ESP_LOGE(s_tag, "Failed to send Button-%d message", button_index + 1);
    }


    // Save state to NVS
    save_button_states_to_nvs();
}

// Button handler functions
void usrButtonPage_button1_handler(void) { handle_button_toggle(0); }
void usrButtonPage_button2_handler(void) { handle_button_toggle(1); }
void usrButtonPage_button3_handler(void) { handle_button_toggle(2); }
void usrButtonPage_button4_handler(void) { handle_button_toggle(3); }
void usrButtonPage_button5_handler(void) { handle_button_toggle(4); }
void usrButtonPage_button6_handler(void) { handle_button_toggle(5); }

bool usrButtonPage_getButtonState(int channel)
{
    if (channel >= 0 && channel < 6)
    {
        return buttonPage.button_states[channel];
    }
    return false;
}

uint32_t usrButtonPage_getSliderValue(int channel)
{
    if (channel >= 0 && channel < 6)
    {
        return sliderPos[channel];
    }
    return 0;
}

// NVS functions (same as original)
esp_err_t save_button_states_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE_BUTTON_STATES, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle for button states: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_BUTTON_STATES, buttonPage.button_states, sizeof(buttonPage.button_states));
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error saving button states: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error committing button states: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(s_tag, "Button states saved to NVS");
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t load_button_states_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE_BUTTON_STATES, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(s_tag, "No saved button states found, using defaults");
        return err;
    }

    size_t required_size = sizeof(buttonPage.button_states);
    err = nvs_get_blob(nvs_handle, NVS_KEY_BUTTON_STATES, buttonPage.button_states, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(s_tag, "No button states found in NVS, using defaults");
    } else {
        ESP_LOGI(s_tag, "Button states loaded from NVS");
        for (int i = 0; i < 6; i++) {
            ESP_LOGI(s_tag, "Button %d state: %s", i+1, buttonPage.button_states[i] ? "ON" : "OFF");
        }
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t save_slider_values_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE_BUTTON_STATES, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle for slider values: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_KEY_SLIDER_VALUES, sliderPos, sizeof(sliderPos));
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error saving slider values: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(s_tag, "Error committing slider values: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(s_tag, "Slider values saved to NVS");
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t load_slider_values_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE_BUTTON_STATES, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(s_tag, "No saved slider values found, using defaults");
        return err;
    }

    size_t required_size = sizeof(sliderPos);
    err = nvs_get_blob(nvs_handle, NVS_KEY_SLIDER_VALUES, sliderPos, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(s_tag, "No slider values found in NVS, using defaults");
    } else {
        ESP_LOGI(s_tag, "Slider values loaded from NVS");
        for (int i = 0; i < 6; i++) {
            buttonPage.slider_values[i] = sliderPos[i];
        }
    }

    nvs_close(nvs_handle);
    return err;
}