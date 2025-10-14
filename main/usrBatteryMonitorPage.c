#include "usrBatteryMonitorPage.h"
#include "usrGeneralDefines.h"
#include "usrBatterySettingsPage.h"
#include "usrBatteryPasswordPage.h"
#include "usrCAN.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>

static const char *s_tag = "usrBatteryMonitorPage";
static usrBatteryMonitorPage_t batteryPage = {0};
static void modal_bg_click_cb(lv_event_t *e);

// TEST MODE - açıp kapanabilir özellik
static bool TEST_MODE = true;  // Test için true, normal kullanım için false

// Corporate color scheme
static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Primary corporate color
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Light background
//static const lv_color_t color_accent = LV_COLOR_MAKE(216, 216, 216);      // Accent gray
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Dark text/borders

// CAN ID definitions for battery sensors
#define BATTERY_REQUEST_ID_BASE 0x300  // 0x300-0x303 requests
#define BATTERY_RESPONSE_ID_BASE 0x300 // 0x300-0x303 responses

#define NVS_NAMESPACE "battery_cfg"
#define NVS_KEY_MAX_VOLTAGE "max_volt_%d"  // %d yerine battery index gelecek

// Default battery names (fallback)
static const char default_battery_names[4][16] = {
    "BATTERY 1",
    "BATTERY 2", 
    "BATTERY 3",
    "BATTERY 4"
};

// Current battery names (loaded from NVS)
static char current_battery_names[4][16];

// Request control flag - stops CAN requests when keyboard is open
static bool getBatteryRequest = true;

// Max voltage values for each battery (initially unset)
static float manual_max_voltages[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// Forward declarations
static void keyboard_event_cb(lv_event_t *e);
static void ta_ready_cb(lv_event_t *e);
static void create_input_popup(void);
static void close_input_popup(void);
static void settings_button_cb(lv_event_t *e);
static void settings_back_callback(void);
static void load_battery_names_from_settings(void);
static void update_battery_display_names(void);
static void password_success_callback(void);
static void password_cancel_callback(void);
static void load_max_voltages_from_nvs(void);
static void save_max_voltage_to_nvs(int battery_index, float max_voltage);

// Modal popup and keyboard
static lv_obj_t *modal_bg = NULL;
static lv_obj_t *input_popup = NULL;
static lv_obj_t *keyboard = NULL;
static lv_obj_t *ta_voltage = NULL;
static int current_battery_index = -1;
static lv_obj_t *settings_button = NULL;

static TickType_t delayTime = 500;

// Test mode için varsayılan max voltage değerleri
static const float default_test_max_voltages[4] = {12.6f, 24.0f, 13.2f, 25.2f};

static void save_max_voltage_to_nvs(int battery_index, float max_voltage)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    
    if (err == ESP_OK) {
        char key[32];
        snprintf(key, sizeof(key), NVS_KEY_MAX_VOLTAGE, battery_index);
        
        err = nvs_set_blob(nvs_handle, key, &max_voltage, sizeof(float));
        if (err == ESP_OK) {
            nvs_commit(nvs_handle);
            ESP_LOGI(s_tag, "Max voltage saved for battery %d: %.2fV", battery_index, max_voltage);
        }
        nvs_close(nvs_handle);
    }
}

static void load_max_voltages_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    
    if (err == ESP_OK) {
        for (int i = 0; i < 4; i++) {
            char key[32];
            snprintf(key, sizeof(key), NVS_KEY_MAX_VOLTAGE, i);
            
            size_t required_size = sizeof(float);
            err = nvs_get_blob(nvs_handle, key, &manual_max_voltages[i], &required_size);
            
            if (err != ESP_OK) {
                // TEST MODE için varsayılan değerleri kullan
                if (TEST_MODE) {
                    manual_max_voltages[i] = default_test_max_voltages[i];
                } else {
                    manual_max_voltages[i] = 0.0f; // Default değer
                }
            }
        }
        nvs_close(nvs_handle);
        ESP_LOGI(s_tag, "Max voltages loaded from NVS");
    }
}

// Load battery names from settings page
static void load_battery_names_from_settings(void)
{
    // Load battery names from NVS via settings page
    usrBatterySettingsPage_loadBatteryNames(current_battery_names);
    
    // Check if any name is empty and use defaults if needed
    for (int i = 0; i < 4; i++) {
        if (strlen(current_battery_names[i]) == 0) {
            strncpy(current_battery_names[i], default_battery_names[i], 16);
            current_battery_names[i][15] = '\0'; // Ensure null termination
        }
    }
    
    ESP_LOGI(s_tag, "Battery names loaded from NVS:");
    for (int i = 0; i < 4; i++) {
        ESP_LOGI(s_tag, "  Battery %d: %s", i + 1, current_battery_names[i]);
    }
}

// Update battery display names on the UI
static void update_battery_display_names(void)
{
    for (int i = 0; i < 4; i++) {
        if (batteryPage.arc_labels[i] != NULL) {
            lv_label_set_text(batteryPage.arc_labels[i], current_battery_names[i]);
            lv_obj_set_style_text_color(batteryPage.arc_labels[i], color_secondary, 0); // Rengi de güncelle
            lv_obj_align_to(batteryPage.arc_labels[i], batteryPage.arc_meters[i], LV_ALIGN_OUT_TOP_MID, 0, -15);
            ESP_LOGI(s_tag, "Updated display name for Battery %d: %s", i + 1, current_battery_names[i]);
        }
    }
}

// Convert ADC to voltage with your voltage divider
static float adc_to_voltage(uint16_t adc_value)
{
    // Voltage divider: R1=560kΩ, R2=41.2kΩ
    // Divider ratio = (R1 + R2) / R2 = (560 + 41.2) / 41.2 = 14.59
    const float voltage_divider_ratio = 14.59f;
    const float adc_reference = 3.3f;
    const float adc_max = 4095.0f;

    float voltage = (adc_value / adc_max) * adc_reference * voltage_divider_ratio;
    return voltage;
}

static void settings_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Settings button clicked - showing password page");
    
    // Settings sayfasının initialize edilip edilmediğini kontrol et
    static bool settings_page_initialized = false;
    static bool password_page_initialized = false;
    
    if (!settings_page_initialized) {
        ESP_LOGI(s_tag, "Settings page not initialized yet, initializing now...");
        
        // Settings sayfasını initialize et
        lv_obj_t *parent = lv_obj_get_parent(batteryPage.page_container);
        if (parent == NULL) {
            parent = lv_scr_act(); // Fallback to active screen
            ESP_LOGW(s_tag, "Using active screen as parent");
        }
        
        usrBatterySettingsPage_init(parent);
        settings_page_initialized = true;
        
        ESP_LOGI(s_tag, "Settings page initialization completed");
    }
    
    // Password sayfasını initialize et (henüz edilmemişse)
    if (!password_page_initialized) {
        ESP_LOGI(s_tag, "Password page not initialized yet, initializing now...");
        
        lv_obj_t *parent = lv_obj_get_parent(batteryPage.page_container);
        if (parent == NULL) {
            parent = lv_scr_act();
        }
        
        usrBatteryPasswordPage_init(parent);
        password_page_initialized = true;
        
        ESP_LOGI(s_tag, "Password page initialization completed");
    }
    
    // Şifre sayfasını göster
    usrBatteryPasswordPage_show(password_success_callback, password_cancel_callback);
    
    ESP_LOGI(s_tag, "Password page shown for battery settings access");
}

static void password_success_callback(void)
{
    ESP_LOGI(s_tag, "Password verified - accessing battery settings");
    
    // Şifre doğruysa ayarlar sayfasını göster
    if (!usrBatterySettingsPage_isActive()) {
        ESP_LOGI(s_tag, "Password correct, showing battery settings page");
        
        // Battery monitor sayfasını gizle
        usrBatteryMonitorPage_hide();
        
        // Ayarlar sayfasını göster
        usrBatterySettingsPage_show();
        
        // Back callback'i ayarla
        usrBatterySettingsPage_setBackCallback(settings_back_callback);
        
        ESP_LOGI(s_tag, "Battery settings page shown after password verification");
    } else {
        ESP_LOGI(s_tag, "Settings page is already active");
    }
}

// Şifre iptali veya yanlış şifre durumunda çağrılacak
static void password_cancel_callback(void)
{
    ESP_LOGI(s_tag, "Password entry cancelled - staying on battery monitor page");
    // Hiçbir şey yapmaya gerek yok, battery monitor sayfası zaten görünür durumda
}

// Settings sayfasından geri dönüş callback'i
static void settings_back_callback(void)
{
    ESP_LOGI(s_tag, "Back from settings page");
    
    // Ayarlar sayfasını gizle
    usrBatterySettingsPage_hide();
    
    // Battery monitor sayfasını tekrar göster
    usrBatteryMonitorPage_show();
    
    // Settings'den döndükten sonra batarya isimlerini yeniden yükle
    ESP_LOGI(s_tag, "Reloading battery names after settings change");
    load_battery_names_from_settings();
    update_battery_display_names();
    
    ESP_LOGI(s_tag, "Returned to battery monitor page with updated names");
}

// Calculate battery percentage based on current voltage and user-defined max voltage
static uint8_t calculate_battery_percentage(float voltage, int battery_index)
{
    uint8_t percentage = 0;
    float max_voltage = manual_max_voltages[battery_index];

    // If max voltage is not set, return 0%
    if (max_voltage <= 0.0f)
    {
        return 0;
    }

    if (voltage >= max_voltage)
    {
        percentage = 100;
    }
    else if (voltage <= 0.0f)
    {
        percentage = 0;
    }
    else
    {
        percentage = (uint8_t)((voltage / max_voltage) * 100.0f);
    }

    return percentage;
}

// Create modal input popup
// Create modal input popup
static void create_input_popup(void)
{
    // Stop CAN requests when keyboard opens
    getBatteryRequest = false;

    if (modal_bg != NULL)
    {
        return; // Already created
    }

    // Create modal background - koyu overlay
    modal_bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(modal_bg, LV_HOR_RES, LV_VER_RES); 
    lv_obj_set_pos(modal_bg, 0, 0);
    lv_obj_set_style_bg_color(modal_bg, lv_color_make(5, 5, 5), 0);  // Çok koyu gri
    lv_obj_set_style_bg_opa(modal_bg, LV_OPA_90, 0);
    lv_obj_set_style_border_width(modal_bg, 0, 0);
    lv_obj_clear_flag(modal_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(modal_bg, modal_bg_click_cb, LV_EVENT_CLICKED, NULL);

    // Create popup container - koyu ana tema
    input_popup = lv_obj_create(modal_bg);
    lv_obj_set_size(input_popup, 500, 320);
    lv_obj_set_pos(input_popup, 130, 130);
    lv_obj_set_style_bg_color(input_popup, lv_color_make(15, 15, 15), 0);  // Koyu gri arka plan
    lv_obj_set_style_border_color(input_popup, lv_color_make(100, 150, 200), 0);  // Mavi border (şifre sayfasından farklı)
    lv_obj_set_style_border_width(input_popup, 3, 0);
    lv_obj_set_style_radius(input_popup, 12, 0);
    lv_obj_clear_flag(input_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(modal_bg, modal_bg_click_cb, LV_EVENT_CLICKED, NULL);

    // Popup container'ın click eventlerinin parent'a bubble etmesini engelle
    lv_obj_add_flag(input_popup, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(input_popup, LV_OBJ_FLAG_EVENT_BUBBLE);

    // Create title label - mavi ton
    lv_obj_t *title_label = lv_label_create(input_popup);
    lv_label_set_text(title_label, "Set Maximum Battery Voltage");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_make(100, 150, 200), 0);  // Mavi başlık
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);

    // Create battery info label - açık gri
    lv_obj_t *info_label = lv_label_create(input_popup);
    if (current_battery_index >= 0)
    {
        lv_label_set_text_fmt(info_label, "Setting max voltage for %s (Current: %.1fV)",
                              current_battery_names[current_battery_index],
                              batteryPage.battery_data[current_battery_index].voltage);
    }
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(info_label, lv_color_make(180, 180, 180), 0);  // Açık gri
    lv_obj_align(info_label, LV_ALIGN_TOP_MID, 0, 30);

    // Create text area - koyu tema
    ta_voltage = lv_textarea_create(input_popup);
    lv_obj_set_size(ta_voltage, 350, 40);
    lv_obj_align(ta_voltage, LV_ALIGN_TOP_MID, 0, 75);
    lv_textarea_set_placeholder_text(ta_voltage, "Enter max voltage (5.0-30.0V)");
    lv_textarea_set_one_line(ta_voltage, true);
    lv_textarea_set_max_length(ta_voltage, 6);
    lv_obj_set_style_text_font(ta_voltage, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(ta_voltage, keyboard_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_voltage, ta_ready_cb, LV_EVENT_READY, NULL);
    
    // Text area styling - koyu tema
    lv_obj_set_style_bg_color(ta_voltage, lv_color_make(30, 30, 30), 0);  // Koyu gri arka plan
    lv_obj_set_style_text_color(ta_voltage, lv_color_make(220, 220, 220), 0);  // Açık yazı
    lv_obj_set_style_border_color(ta_voltage, lv_color_make(100, 150, 200), 0);  // Mavi border
    lv_obj_set_style_border_width(ta_voltage, 2, 0);
    lv_obj_set_style_radius(ta_voltage, 6, 0);
    lv_obj_set_style_pad_all(ta_voltage, 8, 0);

    // Create keyboard - koyu tema
    keyboard = lv_keyboard_create(input_popup);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(keyboard, 450, 150);
    lv_obj_align(keyboard, LV_ALIGN_TOP_MID, 0, 135);
    lv_obj_set_style_radius(keyboard, 8, 0);
    
    // Keyboard styling - koyu tema
    lv_obj_set_style_bg_color(keyboard, lv_color_make(20, 20, 20), 0);  // Koyu arka plan
    lv_obj_set_style_border_color(keyboard, lv_color_make(100, 150, 200), 0);  // Mavi border
    lv_obj_set_style_border_width(keyboard, 2, 0);

    // Link keyboard to text area
    lv_keyboard_set_textarea(keyboard, ta_voltage);

    // Set current max voltage as default text
    if (current_battery_index >= 0)
    {
        if (manual_max_voltages[current_battery_index] > 0.0f)
        {
            char current_voltage[16];
            snprintf(current_voltage, sizeof(current_voltage), "%.1f",
                     manual_max_voltages[current_battery_index]);
            lv_textarea_set_text(ta_voltage, current_voltage);
        }
        else
        {
            lv_textarea_set_text(ta_voltage, "");
        }
        lv_textarea_set_cursor_pos(ta_voltage, LV_TEXTAREA_CURSOR_LAST);
    }
}


static void modal_bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    
    // Eğer tıklanan obje modal background ise popup'ı kapat
    if (target == modal_bg)
    {
        ESP_LOGI(s_tag, "Modal background clicked, closing popup");
        close_input_popup();
        current_battery_index = -1;
    }
}

// Close input popup
static void close_input_popup(void)
{
    if (modal_bg != NULL)
    {
        lv_obj_del(modal_bg);
        modal_bg = NULL;
        input_popup = NULL;
        keyboard = NULL;
        ta_voltage = NULL;
    }
    // Resume CAN requests when keyboard closes
    getBatteryRequest = true;
}

// Keyboard event callback
static void keyboard_event_cb(lv_event_t *e)
{
    // No need to do anything here since keyboard is already linked
}

// Text area ready event callback
static void ta_ready_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);

    // Get entered voltage value
    const char *txt = lv_textarea_get_text(ta);
    float max_voltage = atof(txt);

    // Validate voltage range (5V to 30V)
    if (max_voltage >= 5.0f && max_voltage <= 30.0f && current_battery_index >= 0)
    {
        // Store the max voltage for this battery
        manual_max_voltages[current_battery_index] = max_voltage;

        // NVS'e kaydet
        save_max_voltage_to_nvs(current_battery_index, max_voltage);

        // Recalculate percentage with current voltage and new max
        float current_voltage = batteryPage.battery_data[current_battery_index].voltage;
        uint8_t percentage = calculate_battery_percentage(current_voltage, current_battery_index);

        // Update the battery display
        usrBatteryMonitorPage_updateBattery(current_battery_index, percentage, current_voltage, 0.0f, 0.0f);

        ESP_LOGI(s_tag, "Max voltage set - Battery %d: Current=%.2fV, Max=%.2fV, Percentage=%d%%",
                 current_battery_index + 1, current_voltage, max_voltage, percentage);
    }
    else
    {
        ESP_LOGW(s_tag, "Invalid max voltage value: %.2fV (range: 5.0V - 30.0V)", max_voltage);
    }

    // Close popup
    close_input_popup();
    current_battery_index = -1;
}

// Voltage label click event callback
static void voltage_click_cb(lv_event_t *e)
{
    lv_obj_t *clicked_label = lv_event_get_target(e);

    // Find which battery's voltage label was clicked
    for (int i = 0; i < 4; i++)
    {
        if (batteryPage.voltage_labels[i] == clicked_label) // value_labels yerine voltage_labels
        {
            current_battery_index = i;
            break;
        }
    }

    if (current_battery_index >= 0)
    {
        // Create and show popup
        create_input_popup();
        ESP_LOGI(s_tag, "Voltage input popup opened for Battery %d", current_battery_index + 1);
    }
}

// UPDATED Timer callback 
static void battery_request_timer_cb(lv_timer_t *timer)
{
    // Only send requests if keyboard is not open
    if (getBatteryRequest)
    {
        if (!TEST_MODE) 
        {
            // NORMAL MOD - Gerçek CAN istekleri gönder
            for (int i = 0; i < 4; i++)
            {
                uint32_t request_id = BATTERY_REQUEST_ID_BASE + i;
                esp_err_t result = sendCanHeader(request_id, 0); // Request data

                if (result == ESP_OK)
                {
                    ESP_LOGI(s_tag, "Battery %d data requested (ID: 0x%lX)", i + 1, request_id);
                }
                else
                {
                    ESP_LOGE(s_tag, "Failed to request battery %d data", i + 1);
                }
                vTaskDelay(pdMS_TO_TICKS(delayTime)); // Small delay between requests
            }
        }
        else 
        {
            // TEST MODU - Animasyonlu sahte veriler üret
            static uint32_t test_counter = 0;
            test_counter++;
            
            for (int i = 0; i < 4; i++) 
            {
                float new_voltage;
                uint8_t percentage;
                
                if (i == 0) {
                    // Birinci batarya için fix %5
                    percentage = 5;
                    new_voltage = manual_max_voltages[i] * 0.05f; // Max voltajın %5'i
                } else {
                    // Diğer bataryalar için animasyonlu veriler
                    float base_voltage = manual_max_voltages[i] * 0.85f;
                    float amplitude = manual_max_voltages[i] * 0.15f;
                    float frequency = 0.03f;
                    float phase = i * 0.7f;
                    
                    new_voltage = base_voltage + amplitude * sin((test_counter * frequency) + phase);
                    
                    // Realistik sınırlar
                    float min_voltage = manual_max_voltages[i] * 0.1f;
                    float max_voltage = manual_max_voltages[i] * 0.98f;
                    if (new_voltage < min_voltage) new_voltage = min_voltage;
                    if (new_voltage > max_voltage) new_voltage = max_voltage;
                    
                    percentage = calculate_battery_percentage(new_voltage, i);
                }
                
                // Fake ADC value (for consistency)
                uint16_t fake_adc = (uint16_t)(new_voltage * 4095.0f / 30.0f);
                
                // Display'i güncelle
                usrBatteryMonitorPage_updateBattery(i, percentage, new_voltage, 0.0f, 0.0f);
                
                ESP_LOGI(s_tag, "[TEST MODE] Battery %d: %.2fV (%d%%) - ADC: %d", 
                         i + 1, new_voltage, percentage, fake_adc);
            }
            
            ESP_LOGI(s_tag, "[TEST MODE] All batteries updated with animated test data (counter: %ld)", test_counter);
        }
    }
}

// Process received CAN data for batteries
void usrBatteryMonitorPage_processCanData(uint32_t can_id, uint32_t data)
{
    // TEST MODE'da gerçek CAN verilerini işleme - sadece log
    if (TEST_MODE) {
        ESP_LOGI(s_tag, "[TEST MODE] Ignoring CAN data - ID: 0x%lX, Data: 0x%lX", can_id, data);
        return;
    }
    
    // Check if this is a battery response
    if (can_id >= BATTERY_RESPONSE_ID_BASE && can_id <= (BATTERY_RESPONSE_ID_BASE + 3))
    {
        int battery_index = can_id - BATTERY_RESPONSE_ID_BASE;

        // Extract 12-bit ADC value from CAN data (assuming it's in lower 16 bits)
        uint16_t adc_value = (uint16_t)(data & 0xFFF); // 12-bit mask

        // Convert ADC to voltage
        float voltage = adc_to_voltage(adc_value);

        // Calculate percentage based on user-defined max voltage
        uint8_t percentage = calculate_battery_percentage(voltage, battery_index);

        // Update display
        usrBatteryMonitorPage_updateBattery(battery_index, percentage, voltage, 0.0f, 0.0f);

        ESP_LOGI(s_tag, "Battery %d: ADC=%d, Current=%.2fV, Max=%.2fV, Percentage=%d%%",
                 battery_index + 1, adc_value, voltage, manual_max_voltages[battery_index], percentage);
    }
}

void usrBatteryMonitorPage_init(lv_obj_t *parent)
{
    if (batteryPage.page_container != NULL)
    {
        ESP_LOGW(s_tag, "Battery monitor page already initialized");
        return;
    }

    // Load battery names from NVS first
    ESP_LOGI(s_tag, "Loading battery names during initialization");
    load_battery_names_from_settings();

    // Max voltage değerlerini NVS'den yükle (TEST MODE için varsayılanları içerir)
    load_max_voltages_from_nvs();

     // Create main container
    batteryPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(batteryPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(batteryPage.page_container, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_border_width(batteryPage.page_container, 0, 0);
    lv_obj_clear_flag(batteryPage.page_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create title container for better positioning
    lv_obj_t *title_container = lv_obj_create(batteryPage.page_container);
    lv_obj_set_size(title_container, LV_HOR_RES, 50);
    lv_obj_set_pos(title_container, 0, 0);
    lv_obj_set_style_bg_opa(title_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create title - TEST MODE durumuna göre başlığı güncelle
    batteryPage.title_label = lv_label_create(title_container);
    if (TEST_MODE) {
        lv_label_set_text(batteryPage.title_label, "BATTERY MONITOR [TEST MODE]");
    } else {
        lv_label_set_text(batteryPage.title_label, "BATTERY MONITOR");
    }
    lv_obj_set_style_text_color(batteryPage.title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_text_font(batteryPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_align(batteryPage.title_label, LV_ALIGN_CENTER, -55, -10);

    // Settings button (sağ üst köşe)
    settings_button = lv_btn_create(title_container);
    lv_obj_set_size(settings_button, 40, 40);
    lv_obj_set_pos(settings_button, LV_HOR_RES - 170, -20);
    lv_obj_set_style_bg_color(settings_button, color_primary, 0);
    lv_obj_set_style_radius(settings_button, 5, 0);
    lv_obj_add_event_cb(settings_button, settings_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *settings_label = lv_label_create(settings_button);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(settings_label, color_dark, 0);
    lv_obj_center(settings_label);
    
    // Create arc meters in 1x4 horizontal layout for 800x480 screen
    int arc_size = 155;
    int spacing_x = 30;
    int total_width = (4 * arc_size) + (3 * spacing_x);
    int start_x = (800 - total_width) / 2 - 10;
    int start_y = 130;

    for (int i = 0; i < 4; i++)
    {
        // Calculate position for 1x4 horizontal layout
        int x = start_x + i * (arc_size + spacing_x);
        int y = start_y;

        // Create arc meter
        batteryPage.arc_meters[i] = lv_arc_create(batteryPage.page_container);
        lv_obj_set_size(batteryPage.arc_meters[i], arc_size, arc_size);
        lv_obj_set_pos(batteryPage.arc_meters[i], x - 50, y);

        // Style the arc - kalınlığı arttırıldı
        lv_arc_set_range(batteryPage.arc_meters[i], 0, 100);
        lv_arc_set_value(batteryPage.arc_meters[i], 0);
        lv_arc_set_bg_angles(batteryPage.arc_meters[i], 0, 360);
        lv_obj_remove_style(batteryPage.arc_meters[i], NULL, LV_PART_KNOB);
        lv_obj_clear_flag(batteryPage.arc_meters[i], LV_OBJ_FLAG_CLICKABLE);

        // Set arc colors - kalınlık 15'ten 20'ye çıkarıldı
        lv_obj_set_style_arc_color(batteryPage.arc_meters[i], lv_color_make(128, 128, 128), LV_PART_MAIN);
        lv_obj_set_style_arc_color(batteryPage.arc_meters[i], color_primary, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(batteryPage.arc_meters[i], 20, LV_PART_MAIN);
        lv_obj_set_style_arc_width(batteryPage.arc_meters[i], 20, LV_PART_INDICATOR);

        // Create battery name label (above the arc)
        batteryPage.arc_labels[i] = lv_label_create(batteryPage.page_container);
        lv_label_set_text(batteryPage.arc_labels[i], current_battery_names[i]);
        lv_obj_set_style_text_font(batteryPage.arc_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(batteryPage.arc_labels[i], color_secondary, 0);
        lv_obj_align_to(batteryPage.arc_labels[i], batteryPage.arc_meters[i], LV_ALIGN_OUT_TOP_MID, 0, -15);

        // Create percentage label in center of arc - sadece yüzde, büyük ve kalın font
        batteryPage.value_labels[i] = lv_label_create(batteryPage.page_container);
        lv_label_set_text(batteryPage.value_labels[i], "0\n%%");
        lv_obj_set_style_text_font(batteryPage.value_labels[i], &lv_font_montserrat_24, 0); // Büyük font
        lv_obj_set_style_text_color(batteryPage.value_labels[i], lv_color_white(), 0); // Beyaz renk
        lv_obj_set_style_text_align(batteryPage.value_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align_to(batteryPage.value_labels[i], batteryPage.arc_meters[i], LV_ALIGN_CENTER, 5, 0);

        // Percentage label'ı clickable yapma - artık voltaj labeli olmadığı için
        lv_obj_clear_flag(batteryPage.value_labels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(batteryPage.value_labels[i], LV_OPA_TRANSP, 0);

        // Create voltage info label below the arc - şık tasarım
        batteryPage.voltage_labels[i] = lv_label_create(batteryPage.page_container);
        lv_label_set_text(batteryPage.voltage_labels[i], "0.0V\nClick to set max");
        lv_obj_set_style_text_font(batteryPage.voltage_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(batteryPage.voltage_labels[i], color_secondary, 0);
        lv_obj_set_style_text_align(batteryPage.voltage_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align_to(batteryPage.voltage_labels[i], batteryPage.arc_meters[i], LV_ALIGN_OUT_BOTTOM_MID, 10, 10);

        // Voltage label'ı clickable yap
        lv_obj_add_flag(batteryPage.voltage_labels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(batteryPage.voltage_labels[i], voltage_click_cb, LV_EVENT_CLICKED, NULL);

        // Voltage label için hover efekti
        lv_obj_set_style_bg_color(batteryPage.voltage_labels[i], lv_color_make(10, 10, 10), 0);
        lv_obj_set_style_bg_opa(batteryPage.voltage_labels[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(batteryPage.voltage_labels[i], 6, 0);
        lv_obj_set_style_pad_all(batteryPage.voltage_labels[i], 6, 0);

        // Initialize battery data
        batteryPage.battery_data[i].percentage = 0;
        batteryPage.battery_data[i].voltage = 0.0f;
        batteryPage.battery_data[i].current = 0.0f;
        batteryPage.battery_data[i].temperature = 0.0f;
        batteryPage.battery_data[i].status = "Ready";
    }


    // Create status label - TEST MODE durumuna göre mesajı güncelle
    batteryPage.status_label = lv_label_create(batteryPage.page_container);
    if (TEST_MODE) {
        lv_label_set_text(batteryPage.status_label, "[TEST MODE ACTIVE] Animated test data - Change TEST_MODE to false for real operation");
        lv_obj_set_style_text_color(batteryPage.status_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    } else {
        lv_label_set_text(batteryPage.status_label, "Click voltage values to set maximum levels");
        lv_obj_set_style_text_color(batteryPage.status_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    }
    lv_obj_set_style_text_font(batteryPage.status_label, &lv_font_montserrat_12, 0);
    lv_obj_align(batteryPage.status_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Initially hide the page
    usrBatteryMonitorPage_hide();

    ESP_LOGI(s_tag, "Battery monitor page initialized with TEST_MODE = %s", TEST_MODE ? "ENABLED" : "DISABLED");
}

void usrBatteryMonitorPage_show(void)
{
    if (batteryPage.page_container != NULL)
    {
        lv_obj_clear_flag(batteryPage.page_container, LV_OBJ_FLAG_HIDDEN);
        batteryPage.is_active = true;

        // Sadece display güncellemesi yap, NVS yükleme yapma
        update_battery_display_names();
        
        // Timer'ı başlat
        usrBatteryMonitorPage_startDataCollection();
    }
}

void usrBatteryMonitorPage_hide(void)
{
    if (batteryPage.page_container != NULL)
    {
        lv_obj_add_flag(batteryPage.page_container, LV_OBJ_FLAG_HIDDEN);
        batteryPage.is_active = false;
        usrBatteryMonitorPage_stopDataCollection();

        // Hide popup if open
        close_input_popup();

        ESP_LOGI(s_tag, "Battery monitor page hidden");
    }
}

void usrBatteryMonitorPage_destroy(void)
{
    if (batteryPage.page_container != NULL)
    {
        usrBatteryMonitorPage_stopDataCollection();
        close_input_popup(); // Close any open popup
        
        // Settings button temizliği
        if (settings_button != NULL) {
            settings_button = NULL;
        }
        
        lv_obj_del(batteryPage.page_container);
        memset(&batteryPage, 0, sizeof(usrBatteryMonitorPage_t));
        modal_bg = NULL;
        input_popup = NULL;
        keyboard = NULL;
        ta_voltage = NULL;
        current_battery_index = -1;
        ESP_LOGI(s_tag, "Battery monitor page destroyed");
    }
}

void usrBatteryMonitorPage_updateBattery(int battery_index, uint8_t percentage, float voltage, float current, float temperature)
{
    if (battery_index < 0 || battery_index >= 4)
    {
        ESP_LOGW(s_tag, "Invalid battery index: %d", battery_index);
        return;
    }

    if (batteryPage.arc_meters[battery_index] == NULL)
    {
        return;
    }

    // Update arc value
    lv_arc_set_value(batteryPage.arc_meters[battery_index], percentage);

    // Update percentage label (center of arc) - sadece yüzde
    char percentage_text[8];
    snprintf(percentage_text, sizeof(percentage_text), "%d\n%%", percentage);
    lv_label_set_text(batteryPage.value_labels[battery_index], percentage_text);

    // Update voltage label (below arc) - voltaj bilgileri
    char voltage_text[32];
    if (manual_max_voltages[battery_index] > 0.0f)
    {
        snprintf(voltage_text, sizeof(voltage_text), "%.1fV\nMax: %.1fV", voltage, manual_max_voltages[battery_index]);
    }
    else
    {
        snprintf(voltage_text, sizeof(voltage_text), "%.1fV\nClick to set max", voltage);
    }
    lv_label_set_text(batteryPage.voltage_labels[battery_index], voltage_text);

    // Update battery data
    batteryPage.battery_data[battery_index].percentage = percentage;
    batteryPage.battery_data[battery_index].voltage = voltage;
    batteryPage.battery_data[battery_index].current = current;
    batteryPage.battery_data[battery_index].temperature = temperature;

    // Change color based on percentage (renk değişikliği aynı kalacak)
    lv_color_t color;
    static uint8_t prev_percentage[4] = {0};

    if (percentage >= 70)
    {
        color = color_primary;
    }
    else if (percentage >= 30)
    {
        if (percentage >= prev_percentage[battery_index])
        {
            color = color_primary;
        }
        else
        {
            color = lv_palette_main(LV_PALETTE_ORANGE);
        }
    }
    else
    {
        color = lv_palette_main(LV_PALETTE_RED);
    }

    prev_percentage[battery_index] = percentage;
    lv_obj_set_style_arc_color(batteryPage.arc_meters[battery_index], color, LV_PART_INDICATOR);
}

void usrBatteryMonitorPage_setStatus(const char *status)
{
    if (batteryPage.status_label != NULL && status != NULL)
    {
        lv_label_set_text(batteryPage.status_label, status);
    }
}

bool usrBatteryMonitorPage_isActive(void)
{
    return batteryPage.is_active;
}

void usrBatteryMonitorPage_startDataCollection(void)
{
    if (batteryPage.update_timer == NULL)
    {
        // Start 500ms timer for requesting battery data
        batteryPage.update_timer = lv_timer_create(battery_request_timer_cb, 500, NULL);
        if (TEST_MODE) {
            ESP_LOGI(s_tag, "Battery TEST MODE data collection started (500ms interval)");
            usrBatteryMonitorPage_setStatus("[TEST MODE] Animated test data active");
        } else {
            ESP_LOGI(s_tag, "Battery data collection started (500ms interval)");
            usrBatteryMonitorPage_setStatus("Monitoring batteries - Click voltage to set max levels");
        }
    }
}

void usrBatteryMonitorPage_stopDataCollection(void)
{
    if (batteryPage.update_timer != NULL)
    {
        lv_timer_del(batteryPage.update_timer);
        batteryPage.update_timer = NULL;
        ESP_LOGI(s_tag, "Battery data collection stopped");
        usrBatteryMonitorPage_setStatus("Data collection stopped");
    }
}

// Public function to refresh battery names from settings (can be called externally)
void usrBatteryMonitorPage_refreshBatteryNames(void)
{
    ESP_LOGI(s_tag, "Refreshing battery names (external call)");
    load_battery_names_from_settings();
    update_battery_display_names();
}

// YENİ: Test modunu açıp kapatmak için fonksiyon
void usrBatteryMonitorPage_setTestMode(bool enable)
{
    bool was_test_mode = TEST_MODE;
    TEST_MODE = enable;
    
    if (was_test_mode != TEST_MODE) {
        ESP_LOGI(s_tag, "Test mode changed from %s to %s", 
                 was_test_mode ? "ENABLED" : "DISABLED",
                 TEST_MODE ? "ENABLED" : "DISABLED");
        
        // Başlığı güncelle
        if (batteryPage.title_label != NULL) {
            if (TEST_MODE) {
                lv_label_set_text(batteryPage.title_label, "BATTERY MONITOR [TEST MODE]");
                lv_obj_set_style_text_color(batteryPage.title_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
            } else {
                lv_label_set_text(batteryPage.title_label, "BATTERY MONITOR");
                lv_obj_set_style_text_color(batteryPage.title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
            }
        }
        
        // Status mesajını güncelle
        if (batteryPage.status_label != NULL) {
            if (TEST_MODE) {
                lv_label_set_text(batteryPage.status_label, "[TEST MODE ACTIVE] Animated test data - Real sensors ignored");
                lv_obj_set_style_text_color(batteryPage.status_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
            } else {
                lv_label_set_text(batteryPage.status_label, "Click voltage values to set maximum levels");
                lv_obj_set_style_text_color(batteryPage.status_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
            }
        }
        
        // Eğer TEST MODE'a geçiliyorsa max voltage değerlerini varsayılan test değerleriyle güncelle
        if (TEST_MODE) {
            for (int i = 0; i < 4; i++) {
                if (manual_max_voltages[i] <= 0.0f) {
                    manual_max_voltages[i] = default_test_max_voltages[i];
                }
            }
        }
    }
}

// YENİ: Test modu durumunu öğrenmek için fonksiyon
bool usrBatteryMonitorPage_isTestMode(void)
{
    return TEST_MODE;
}

// Legacy compatibility functions (redirect to data collection)
void usrBatteryMonitorPage_startSimulation(void)
{
    usrBatteryMonitorPage_startDataCollection();
}

void usrBatteryMonitorPage_stopSimulation(void)
{
    usrBatteryMonitorPage_stopDataCollection();
}