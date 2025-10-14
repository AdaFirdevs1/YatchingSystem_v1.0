#include "usrShuntPage.h"
#include "usrCAN.h"
#include "usrGeneralDefines.h"
#include "usrShuntSettingsPage.h"
#include "usrShuntPasswordPage.h"
#include "esp_log.h"
#include <math.h>

// Test modu - tüm shunt'ları görüntülemek için
static bool TEST_MODE = true;  // Test için true, normal kullanım için false

static const char *s_tag = "usrShuntPage";
static usrShuntPage_t shuntPage = {0};

// CAN ID definitions - STM32 ile uyumlu
#define QUADRO_SHUNT_REQUEST_ID_BASE 0x100  // 0x100-0x103 requests
#define QUADRO_SHUNT_RESPONSE_ID_BASE 0x180 // 0x180-0x183 responses
#define MAIN_SHUNT_REQUEST_ID 0x200         // STM32'den veri isteme
#define MAIN_SHUNT_RESPONSE_ID 0x280        // STM32'den gelen yanıt

// ADC to voltage conversion constants
#define ADC_MAX_VALUE 4095.0f       // 12-bit ADC
#define ADC_REFERENCE_VOLTAGE 3.3f  // ESP32 ADC referans voltajı
#define VOLTAGE_DIVIDER_RATIO 11.0f // (100K + 10K) / 10K = 11

// Shunt current calculation constants
#define SHUNT_RESISTANCE_MOHM 75.0f                                      // 75 mΩ shunt direnci
#define SHUNT_VOLTAGE_TO_CURRENT_RATIO (1000.0f / SHUNT_RESISTANCE_MOHM) // mV to A conversion

static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Ana turkuaz
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Açık beyaz
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Koyu gri
static const lv_color_t color_background = LV_COLOR_MAKE(10, 10, 10);     // Ana arka plan


// 7 inç ekran için parlak renkler
static const lv_color_t shunt_colors[5] = {
    LV_COLOR_MAKE(146, 193, 193),   // Main Shunt - Ana turkuaz
    LV_COLOR_MAKE(100, 180, 220),   // Quadro 1 - Açık mavi  
    LV_COLOR_MAKE(180, 220, 180),   // Quadro 2 - Açık yeşil
    LV_COLOR_MAKE(220, 180, 120),   // Quadro 3 - Altın sarısı
    LV_COLOR_MAKE(200, 160, 200)    // Quadro 4 - Açık mor
};

// Battery colors
static const lv_color_t battery_colors[2] = {
    LV_COLOR_MAKE(100, 145, 145),   // Battery 1 - Daha koyu turkuaz
    LV_COLOR_MAKE(50, 130, 170)     // Battery 2 - Daha koyu mavi
};



static TickType_t delayTime = 300; // CAN istekleri arası gecikme

// Forward declarations
static void shunt_request_timer_cb(lv_timer_t *timer);
static void request_shunt_data(void);
static float calculate_power(float current, float voltage);
static lv_color_t get_current_color(float current, float max_current);
static lv_color_t get_battery_color(float voltage, float min_voltage, float max_voltage);
static float convert_adc_to_voltage(uint32_t adc_value);
static float convert_adc_to_current(uint32_t adc_value);
static void settings_button_cb(lv_event_t *e);
static void settings_back_callback(void);
static void load_shunt_settings(void);
static void apply_shunt_visibility(void);
static void update_shunt_labels(void);
static void password_success_callback(void);
static void password_cancel_callback(void);

// Convert ADC value to actual voltage (considering voltage divider)
static float convert_adc_to_voltage(uint32_t adc_value)
{
    // ADC değerini voltaja çevir
    float adc_voltage = ((float)adc_value / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;

    // Gerilim bölücü ile gerçek voltajı hesapla
    float actual_voltage = adc_voltage * VOLTAGE_DIVIDER_RATIO;

    return actual_voltage;
}

// Convert shunt ADC value to current (A)
static float convert_adc_to_current(uint32_t adc_value)
{
    // ADC değerini voltaja çevir (shunt üzerindeki gerilim düşümü)
    float shunt_voltage_mv = ((float)adc_value / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE * 1000.0f; // mV

    // Shunt akımını hesapla: I = V / R (A = mV / mΩ)
    float current = shunt_voltage_mv / SHUNT_RESISTANCE_MOHM;

    return current;
}

// Timer callback for requesting shunt data
static void shunt_request_timer_cb(lv_timer_t *timer)
{
    if (shuntPage.getShuntRequest)
    {
        if (!TEST_MODE) 
        {
            // NORMAL MOD - Gerçek sensör kontrolü
            request_shunt_data();
        } 
        else 
        {
            // TEST MODU - Sahte animasyonlu veriler
            static uint32_t test_counter = 0;
            test_counter++;
            
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Main Shunt test data - sinüs dalgası ile animasyonlu akım
            if (!shuntPage.main_shunt.is_connected) {
                shuntPage.main_shunt.is_connected = true;
            }
            
            float base_current = 50.0f;  // Orta seviye akım
            float amplitude = 40.0f;     // Salınım genliği  
            float frequency = 0.03f;     // Yavaş animasyon
            
            float new_current = base_current + amplitude * sin(test_counter * frequency);
            if (new_current < 0.0f) new_current = 0.0f;
            if (new_current > 280.0f) new_current = 280.0f;
            
            shuntPage.main_shunt.current = new_current;
            shuntPage.main_shunt.voltage = 12.5f + (sin(test_counter * 0.02f) * 1.0f); // 11.5V - 13.5V arası
            shuntPage.main_shunt.power = shuntPage.main_shunt.current * shuntPage.main_shunt.voltage;
            shuntPage.main_shunt.last_data_time = current_time;
            
            // Quadro Shunts test data - her biri farklı faz ve genlik
            for (int i = 0; i < 4; i++) {
                if (!shuntPage.quadro_shunts[i].is_connected) {
                    shuntPage.quadro_shunts[i].is_connected = true;
                }
                
                float phase = i * 1.5f;  // Her quadro farklı faz
                float quadro_base = 10.0f + (i * 5.0f);  // Her quadro farklı orta seviye
                float quadro_amplitude = 8.0f + (i * 2.0f);  // Farklı genlikler
                
                float quadro_current = quadro_base + quadro_amplitude * sin((test_counter * 0.04f) + phase);
                if (quadro_current < 0.0f) quadro_current = 0.0f;
                if (quadro_current > 28.0f) quadro_current = 28.0f;
                
                shuntPage.quadro_shunts[i].current = quadro_current;
                shuntPage.quadro_shunts[i].voltage = 12.0f + (sin((test_counter * 0.025f) + phase) * 0.8f);
                shuntPage.quadro_shunts[i].power = shuntPage.quadro_shunts[i].current * shuntPage.quadro_shunts[i].voltage;
                shuntPage.quadro_shunts[i].last_data_time = current_time;
                
                // Display güncelle
                usrShuntPage_updateQuadroShuntDisplay(i);
            }
            
            // Battery test data - yavaş değişen voltajlar
            for (int i = 0; i < 2; i++) {
                if (!shuntPage.batteries[i].is_connected) {
                    shuntPage.batteries[i].is_connected = true;
                }
                
                float battery_base = 12.2f + (i * 0.3f);  // Farklı temel voltajlar
                float battery_variation = sin((test_counter * 0.01f) + (i * 2.0f)) * 0.5f;
                
                shuntPage.batteries[i].voltage = battery_base + battery_variation;
                if (shuntPage.batteries[i].voltage < 10.8f) shuntPage.batteries[i].voltage = 10.8f;
                if (shuntPage.batteries[i].voltage > 14.0f) shuntPage.batteries[i].voltage = 14.0f;
                
                shuntPage.batteries[i].last_data_time = current_time;
                
                // Display güncelle
                usrShuntPage_updateBatteryDisplay(i);
            }
            
            // Main shunt display güncelle
            usrShuntPage_updateMainShuntDisplay();
            
            ESP_LOGI(s_tag, "[TEST MODE] All shunts updated with animated test data (counter: %ld)", test_counter);
        }
    }
}

// Request data from all shunts
static void request_shunt_data(void)
{
    // Check for sensor timeouts (no data for 3 seconds)
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Check main shunt timeout
    if (shuntPage.main_shunt.is_connected)
    {
        if (current_time - shuntPage.main_shunt.last_data_time > 3000)
        {
            shuntPage.main_shunt.is_connected = false;
            ESP_LOGW(s_tag, "Main shunt timeout - disconnected");
        }
    }

    // Check battery timeouts
    for (int i = 0; i < 2; i++)
    {
        if (shuntPage.batteries[i].is_connected)
        {
            if (current_time - shuntPage.batteries[i].last_data_time > 3000)
            {
                shuntPage.batteries[i].is_connected = false;
                ESP_LOGW(s_tag, "Battery %d timeout - disconnected", i + 1);
            }
        }
    }

    // Check quadro shunts timeout
    for (int i = 0; i < 4; i++)
    {
        if (shuntPage.quadro_shunts[i].is_connected)
        {
            if (current_time - shuntPage.quadro_shunts[i].last_data_time > 3000)
            {
                shuntPage.quadro_shunts[i].is_connected = false;
                ESP_LOGW(s_tag, "Quadro shunt %d timeout - disconnected", i + 1);
            }
        }
    }

    // Request Quadro Shunt data first (eğer varsa)
    for (int i = 0; i < 4; i++)
    {
        uint32_t request_id_quadro = QUADRO_SHUNT_REQUEST_ID_BASE + i;
        esp_err_t result1 = sendCanHeader(request_id_quadro, 0);

        if (result1 == ESP_OK)
        {
            ESP_LOGI(s_tag, "Quadro Shunt %d data requested (ID: 0x%lX)", i + 1, request_id_quadro);
        }
        else
        {
            ESP_LOGE(s_tag, "Failed to request Quadro Shunt %d data", i + 1);
        }
        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
    vTaskDelay(pdMS_TO_TICKS(delayTime));

    // STM32'den ana shunt ve batarya verilerini iste
    uint32_t request_id_main = MAIN_SHUNT_REQUEST_ID;
    esp_err_t result2 = sendCanHeader(request_id_main, 0);
    if (result2 == ESP_OK)
    {
        ESP_LOGI(s_tag, "STM32 Main Shunt data requested (ID: 0x%lX)", request_id_main);
    }
    else
    {
        ESP_LOGE(s_tag, "Failed to request STM32 Main Shunt data");
    }
}

// Calculate power from current and voltage
static float calculate_power(float current, float voltage)
{
    return current * voltage;
}

// Şifre iptal edildiğinde veya yanlış girildiğinde çalışacak callback
static void password_cancel_callback(void) {
    ESP_LOGI(s_tag, "Password cancelled or incorrect - staying on shunt monitor page");
    // Hiçbir şey yapmaya gerek yok, şifre sayfası zaten kapanıyor
}

// Şifre doğru girildiğinde çalışacak callback
static void password_success_callback(void) {
    ESP_LOGI(s_tag, "Password verified - proceeding to shunt settings");
    
    // Shunt settings sayfasının initialize edilip edilmediğini kontrol et
    static bool settings_page_initialized = false;
    if (!settings_page_initialized) {
        ESP_LOGI(s_tag, "Shunt settings page not initialized yet, initializing now...");
        
        // Shunt settings sayfasını initialize et
        lv_obj_t *parent = lv_obj_get_parent(shuntPage.page_container);
        if (parent == NULL) {
            parent = lv_scr_act(); // Fallback to active screen
            ESP_LOGW(s_tag, "Using active screen as parent");
        }
        
        usrShuntSettingsPage_init(parent);
        settings_page_initialized = true;
        ESP_LOGI(s_tag, "Shunt settings page initialization completed");
    }
    
    // Şimdi normal geçiş işlemini yap
    if (!usrShuntSettingsPage_isActive()) {
        ESP_LOGI(s_tag, "Shunt settings page is not active, showing it now");
        
        // Ana shunt sayfasını gizle
        ESP_LOGI(s_tag, "Hiding shunt monitor page");
        usrShuntPage_hide();
        
        // Ayarlar sayfasını göster
        ESP_LOGI(s_tag, "Showing settings page");
        usrShuntSettingsPage_show();
        
        // Back callback'i ayarla
        ESP_LOGI(s_tag, "Setting back callback");
        usrShuntSettingsPage_setBackCallback(settings_back_callback);
        
        ESP_LOGI(s_tag, "Shunt settings page transition completed after password verification");
    } else {
        ESP_LOGI(s_tag, "Shunt settings page is already active");
    }
}

// Settings button callback fonksiyonu
static void settings_button_cb(lv_event_t *e) {
    ESP_LOGI(s_tag, "Settings button clicked - requesting password");
    
    // Şifre sayfasını initialize et (eğer edilmediyse)
    static bool password_page_initialized = false;
    if (!password_page_initialized) {
        usrShuntPasswordPage_init(lv_obj_get_parent(shuntPage.page_container));
        password_page_initialized = true;
        ESP_LOGI(s_tag, "Shunt password page initialized");
    }
    
    // Şifre sayfasını göster
    usrShuntPasswordPage_show(password_success_callback, password_cancel_callback);
}

// Settings sayfasından dönüş callback'i
static void settings_back_callback(void)
{
    ESP_LOGI(s_tag, "Returning from settings page");
    
    // Ayarlar sayfasını gizle
    usrShuntSettingsPage_hide();
    
    // Ayarları yükle ve uygula
    load_shunt_settings();
    //apply_shunt_visibility();
    update_shunt_labels();
    
    // Ana sayfayı göster
    usrShuntPage_show();
}

// Shunt ayarlarını NVS'den yükle
static void load_shunt_settings(void)
{
    // Shunt isimlerini yükle
    usrShuntSettingsPage_loadShuntNames(shuntPage.custom_shunt_names);
    
    
    ESP_LOGI(s_tag, "Shunt settings loaded from NVS");
}

// Shunt görünürlüğünü ayarlara göre uygula
static void apply_shunt_visibility(void)
{
    // Main Shunt
    if (shuntPage.main_shunt_container) {
        if (shuntPage.selected_shunts[0]) {
            lv_obj_clear_flag(shuntPage.main_shunt_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(shuntPage.main_shunt_container, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // Battery containers - Main shunt seçili değilse gizle
    for (int i = 0; i < 2; i++) {
        if (shuntPage.battery_containers[i]) {
            if (shuntPage.selected_shunts[0]) {
                lv_obj_clear_flag(shuntPage.battery_containers[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(shuntPage.battery_containers[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    
    // Quadro Shunts
    for (int i = 0; i < 4; i++) {
        ESP_LOGI(s_tag, "Quadro %d selected: %s", i+1, shuntPage.selected_shunts[i + 1] ? "true" : "false");
        
        if (shuntPage.quadro_containers[i]) {
            if (shuntPage.selected_shunts[i + 1]) {
                lv_obj_clear_flag(shuntPage.quadro_containers[i], LV_OBJ_FLAG_HIDDEN);
                ESP_LOGI(s_tag, "Quadro %d SHOWN", i+1);
            } else {
                lv_obj_add_flag(shuntPage.quadro_containers[i], LV_OBJ_FLAG_HIDDEN);
                ESP_LOGI(s_tag, "Quadro %d HIDDEN", i+1);
            }
        }
    }
    
    ESP_LOGI(s_tag, "Shunt visibility applied based on settings");
}

// Shunt etiketlerini güncelle
static void update_shunt_labels(void)
{
    // Main Shunt label güncelle
    if (shuntPage.main_shunt_label) {
        char main_label_text[50];
        snprintf(main_label_text, sizeof(main_label_text), "%s (300A MAX)", 
                 shuntPage.custom_shunt_names[0]);
        lv_label_set_text(shuntPage.main_shunt_label, main_label_text);
    }
    
    // Quadro Shunt labelları güncelle
    for (int i = 0; i < 4; i++) {
        if (shuntPage.quadro_labels[i]) {
            char quadro_label_text[50];
            snprintf(quadro_label_text, sizeof(quadro_label_text), "%s\n(30A MAX)", 
                     shuntPage.custom_shunt_names[i + 1]);
            lv_label_set_text(shuntPage.quadro_labels[i], quadro_label_text);
        }
    }
    
    ESP_LOGI(s_tag, "Shunt labels updated with custom names");
}

// Get color based on current level
static lv_color_t get_current_color(float current, float max_current)
{
    float percentage = (current / max_current) * 100.0f;

    if (percentage >= 90.0f)
    {
        return lv_palette_lighten(LV_PALETTE_RED, 1); // Soluk kırmızı
    }
    
    if (percentage >= 70.0f)
    {
        return lv_palette_lighten(LV_PALETTE_ORANGE, 2); // Soluk turuncu
    }
    /*
    if (percentage >= 50.0f)
    {
        return lv_palette_lighten(LV_PALETTE_YELLOW, 2); // Soluk sarı
    }
    */
    return color_primary;
}

static lv_color_t get_battery_color(float voltage, float min_voltage, float max_voltage)
{
    float percentage = ((voltage - min_voltage) / (max_voltage - min_voltage)) * 100.0f;

    if (percentage >= 80.0f)
    {
        return color_primary;
    }
    /*
    if (percentage >= 60.0f)
    {
        return lv_palette_lighten(LV_PALETTE_GREEN, 2); // Soluk yeşil
    }
    
    if (percentage >= 40.0f)
    {
        return lv_palette_lighten(LV_PALETTE_YELLOW, 2); // Soluk sarı
    }
    */
    if (percentage >= 20.0f)
    {
        return lv_palette_lighten(LV_PALETTE_ORANGE, 2); // Soluk turuncu
    }
    
    return lv_palette_lighten(LV_PALETTE_RED, 1); // Soluk kırmızı
}


void usrShuntPage_init(lv_obj_t *parent)
{
    if (shuntPage.page_container != NULL)
    {
        ESP_LOGW(s_tag, "Shunt page already initialized");
        return;
    }

    // Initialize page structure
    memset(&shuntPage, 0, sizeof(usrShuntPage_t));

    // DEFAULT OLARAK BÜTÜN SHUNTLARI SEÇİLİ YAP
    for (int i = 0; i < 5; i++) {
        shuntPage.selected_shunts[i] = true;  // BUNU EKLEYİN
    }

    // Create main container
    shuntPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(shuntPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(shuntPage.page_container, color_background, 0);
    lv_obj_set_style_border_width(shuntPage.page_container, 0, 0);
    lv_obj_clear_flag(shuntPage.page_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(shuntPage.page_container, -60, 0);

    // Create title - 7 inç için büyük font
    shuntPage.title_label = lv_label_create(shuntPage.page_container);
    if (TEST_MODE) {
        lv_label_set_text(shuntPage.title_label, "SHUNT CURRENT MONITOR [TEST MODE]");
    } else {
        lv_label_set_text(shuntPage.title_label, "SHUNT CURRENT MONITOR");
    }
    lv_obj_set_style_text_font(shuntPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(shuntPage.title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(shuntPage.title_label, LV_ALIGN_TOP_MID, 20, 0);


    // Settings button (sağ üst köşe)
    shuntPage.settings_button = lv_btn_create(shuntPage.page_container);
    lv_obj_set_size(shuntPage.settings_button, 40, 40);
    lv_obj_set_pos(shuntPage.settings_button, LV_HOR_RES - 90, -10);
    lv_obj_set_style_bg_color(shuntPage.settings_button, color_primary, 0);
    lv_obj_set_style_radius(shuntPage.settings_button, 5, 0);
    lv_obj_add_event_cb(shuntPage.settings_button, settings_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(shuntPage.settings_button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *settings_label = lv_label_create(shuntPage.settings_button);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(settings_label, color_dark, 0);
    lv_obj_center(settings_label);

    // Ayarları yükle ve uygula (init sonunda)
    load_shunt_settings();
    //apply_shunt_visibility();
    update_shunt_labels();

    // Initialize Main Shunt data
    shuntPage.main_shunt.resistance_mohm = SHUNT_RESISTANCE_MOHM;
    shuntPage.main_shunt.max_current = 300.0f;
    snprintf(shuntPage.main_shunt.name, sizeof(shuntPage.main_shunt.name), "MAIN SHUNT");

    // Initialize Battery data - 12V lead acid battery voltage ranges
    for (int i = 0; i < 2; i++)
    {
        shuntPage.batteries[i].min_voltage = 10.5f; // Minimum safe discharge voltage
        shuntPage.batteries[i].max_voltage = 14.4f; // Full charge voltage
        snprintf(shuntPage.batteries[i].name, sizeof(shuntPage.batteries[i].name), "BATTERY %d", i + 1);
    }

    // Create Main Shunt container (yatay - üstte) - ortalanmış
    shuntPage.main_shunt_container = lv_obj_create(shuntPage.page_container);
    lv_obj_set_size(shuntPage.main_shunt_container, 450, 110);
    lv_obj_align(shuntPage.main_shunt_container, LV_ALIGN_TOP_MID, 20, 55);
    lv_obj_set_style_bg_color(shuntPage.main_shunt_container, color_dark, 0);
    lv_obj_set_style_border_color(shuntPage.main_shunt_container, shunt_colors[0], 0);
    lv_obj_set_style_border_width(shuntPage.main_shunt_container, 3, 0);
    lv_obj_set_style_radius(shuntPage.main_shunt_container, 12, 0);
    lv_obj_clear_flag(shuntPage.main_shunt_container, LV_OBJ_FLAG_SCROLLABLE);

    // Main Shunt label
    shuntPage.main_shunt_label = lv_label_create(shuntPage.main_shunt_container);
    lv_label_set_text(shuntPage.main_shunt_label, "MAIN SHUNT (300A MAX)");
    lv_obj_set_style_text_font(shuntPage.main_shunt_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(shuntPage.main_shunt_label, shunt_colors[0], 0);
    lv_obj_align(shuntPage.main_shunt_label, LV_ALIGN_TOP_MID, 0, 0);

    // Main Shunt bar (yatay)
    shuntPage.main_shunt_bar = lv_bar_create(shuntPage.main_shunt_container);
    lv_obj_set_size(shuntPage.main_shunt_bar, 550, 25);
    lv_obj_align(shuntPage.main_shunt_bar, LV_ALIGN_CENTER, 0, 30);
    lv_bar_set_range(shuntPage.main_shunt_bar, 0, 300); // 300A max
    lv_bar_set_value(shuntPage.main_shunt_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(shuntPage.main_shunt_bar, lv_color_make(50, 50, 50), LV_PART_MAIN);
    lv_obj_set_style_bg_color(shuntPage.main_shunt_bar, shunt_colors[0], LV_PART_INDICATOR);
    lv_obj_set_style_radius(shuntPage.main_shunt_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(shuntPage.main_shunt_bar, 5, LV_PART_INDICATOR);

    // Main Shunt value label
    shuntPage.main_shunt_value_label = lv_label_create(shuntPage.main_shunt_container);
    lv_label_set_text(shuntPage.main_shunt_value_label, "0.0A | 0.0V | 0.0W");
    lv_obj_set_style_text_font(shuntPage.main_shunt_value_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(shuntPage.main_shunt_value_label, color_secondary, 0);
    lv_obj_align(shuntPage.main_shunt_value_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Create Battery Arc Graphics (Sol ve sağ tarafa)
    for (int i = 0; i < 2; i++)
    {
        // Battery container
        shuntPage.battery_containers[i] = lv_obj_create(shuntPage.page_container);
        lv_obj_set_size(shuntPage.battery_containers[i], 120, 120);

        // Sol batarya (Battery 1) - Main Shunt'ın soluna
        // Sağ batarya (Battery 2) - Main Shunt'ın sağına
        int x_offset = (i == 0) ? -350 + 55 : 280 + 55; // Sol: -290, Sağ: +340
        lv_obj_align(shuntPage.battery_containers[i], LV_ALIGN_TOP_MID, x_offset, 50);

        lv_obj_set_style_bg_color(shuntPage.battery_containers[i], color_dark, 0);
        lv_obj_set_style_border_color(shuntPage.battery_containers[i], battery_colors[i], 0);
        lv_obj_set_style_border_width(shuntPage.battery_containers[i], 2, 0);
        lv_obj_set_style_radius(shuntPage.battery_containers[i], 60, 0); // Yuvarlak container
        lv_obj_clear_flag(shuntPage.battery_containers[i], LV_OBJ_FLAG_SCROLLABLE);

        // Battery Arc (Yay grafiği)
        shuntPage.battery_arcs[i] = lv_arc_create(shuntPage.battery_containers[i]);
        lv_obj_set_size(shuntPage.battery_arcs[i], 100, 100);
        lv_obj_align(shuntPage.battery_arcs[i], LV_ALIGN_CENTER, 0, 0);

        // Arc ayarları
        lv_arc_set_range(shuntPage.battery_arcs[i], 0, 100); // 0-100% için
        lv_arc_set_value(shuntPage.battery_arcs[i], 0);
        lv_arc_set_bg_angles(shuntPage.battery_arcs[i], 135, 45); // 270 derece yay
        
        lv_obj_clear_flag(shuntPage.battery_arcs[i], LV_OBJ_FLAG_CLICKABLE);


        // Arc stilleri
        lv_obj_set_style_arc_color(shuntPage.battery_arcs[i], lv_color_make(50, 50, 50), LV_PART_MAIN);
        lv_obj_set_style_arc_width(shuntPage.battery_arcs[i], 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(shuntPage.battery_arcs[i], battery_colors[i], LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(shuntPage.battery_arcs[i], 12, LV_PART_INDICATOR);

        // Knob'u gizle
        lv_obj_set_style_bg_opa(shuntPage.battery_arcs[i], LV_OPA_TRANSP, LV_PART_KNOB);

        // Battery label
        shuntPage.battery_labels[i] = lv_label_create(shuntPage.battery_containers[i]);
        lv_label_set_text_fmt(shuntPage.battery_labels[i], "%s",
                              (i == 0) ? "MAIN BAT" : "BACKUP BAT");
        lv_obj_set_style_text_font(shuntPage.battery_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(shuntPage.battery_labels[i], battery_colors[i], 0);
        lv_obj_align(shuntPage.battery_labels[i], LV_ALIGN_TOP_MID, 0, 0);

        // Battery voltage label (center of arc)
        shuntPage.battery_voltage_labels[i] = lv_label_create(shuntPage.battery_containers[i]);
        lv_label_set_text(shuntPage.battery_voltage_labels[i], "0.0V");
        lv_obj_set_style_text_font(shuntPage.battery_voltage_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(shuntPage.battery_voltage_labels[i], color_secondary, 0);
        lv_obj_align(shuntPage.battery_voltage_labels[i], LV_ALIGN_CENTER, 0, 0);

        // Battery percentage label
        shuntPage.battery_percentage_labels[i] = lv_label_create(shuntPage.battery_containers[i]);
        lv_label_set_text(shuntPage.battery_percentage_labels[i], "0%");
        lv_obj_set_style_text_font(shuntPage.battery_percentage_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(shuntPage.battery_percentage_labels[i], battery_colors[i], 0);
        lv_obj_align(shuntPage.battery_percentage_labels[i], LV_ALIGN_BOTTOM_MID, 0, 5);

        // TEST MODU - Başlangıçta tüm shunt'ları bağlı olarak işaretle
        if (TEST_MODE) 
        {
            ESP_LOGI(s_tag, "TEST MODE ACTIVE - Initializing all shunts as connected");
            
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Main shunt'ı bağlı olarak işaretle
            shuntPage.main_shunt.is_connected = true;
            shuntPage.main_shunt.current = 45.0f;
            shuntPage.main_shunt.voltage = 12.3f;
            shuntPage.main_shunt.power = shuntPage.main_shunt.current * shuntPage.main_shunt.voltage;
            shuntPage.main_shunt.last_data_time = current_time;
            
            // Quadro shunt'ları bağlı olarak işaretle
            for (int i = 0; i < 4; i++) {
                shuntPage.quadro_shunts[i].is_connected = true;
                shuntPage.quadro_shunts[i].current = 15.0f + (i * 3.0f);
                shuntPage.quadro_shunts[i].voltage = 12.1f + (i * 0.1f);
                shuntPage.quadro_shunts[i].power = shuntPage.quadro_shunts[i].current * shuntPage.quadro_shunts[i].voltage;
                shuntPage.quadro_shunts[i].last_data_time = current_time;
            }
            
            // Battery'leri bağlı olarak işaretle
            for (int i = 0; i < 2; i++) {
                shuntPage.batteries[i].is_connected = true;
                shuntPage.batteries[i].voltage = 12.4f + (i * 0.2f);
                shuntPage.batteries[i].last_data_time = current_time;
            }
            
            usrShuntPage_setStatus("TEST MODE - All shunts initialized with animated test data");
        }
        else 
        {
            usrShuntPage_setStatus("Scanning for shunt sensors...");
        }
    }

    // Initialize and create Quadro Shunts (4 dikey sütun - altta) - ortalanmış ve daha küçük
    for (int i = 0; i < 4; i++)
    {
        
        // Initialize Quadro Shunt data
        shuntPage.quadro_shunts[i].resistance_mohm = 75.0f;
        shuntPage.quadro_shunts[i].max_current = 30.0f;
        snprintf(shuntPage.quadro_shunts[i].name, sizeof(shuntPage.quadro_shunts[i].name), "QUADRO %d", i + 1);

        // Calculate position for 4 columns - daha ortalanmış
        int container_width = 160;
        int spacing_x = 20;
        int total_width = (4 * container_width) + (3 * spacing_x);
        int start_x = (800 - total_width) / 2 - 10; // Ekran genişliğinde ortalanmış
        int x = start_x + i * (container_width + spacing_x);

        // Create Quadro Shunt container - daha yukarıda
        shuntPage.quadro_containers[i] = lv_obj_create(shuntPage.page_container);
        lv_obj_set_size(shuntPage.quadro_containers[i], container_width + 10, 150);
        lv_obj_set_pos(shuntPage.quadro_containers[i], x , 190); // x + 10
        lv_obj_set_style_bg_color(shuntPage.quadro_containers[i], color_dark, 0);
        lv_obj_set_style_border_color(shuntPage.quadro_containers[i], shunt_colors[i + 1], 0);
        lv_obj_set_style_border_width(shuntPage.quadro_containers[i], 3, 0);
        lv_obj_set_style_radius(shuntPage.quadro_containers[i], 12, 0);
        lv_obj_clear_flag(shuntPage.quadro_containers[i], LV_OBJ_FLAG_SCROLLABLE);

        // Quadro Shunt label
        shuntPage.quadro_labels[i] = lv_label_create(shuntPage.quadro_containers[i]);
        lv_label_set_text_fmt(shuntPage.quadro_labels[i], "QUADRO %d\n(30A MAX)", i + 1);
        lv_obj_set_style_text_font(shuntPage.quadro_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(shuntPage.quadro_labels[i], shunt_colors[i + 1], 0);
        lv_obj_set_style_text_align(shuntPage.quadro_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(shuntPage.quadro_labels[i], LV_ALIGN_TOP_MID, 0, -10);

        // Quadro Shunt bar (dikey) - biraz daha küçük
        shuntPage.quadro_bars[i] = lv_bar_create(shuntPage.quadro_containers[i]);
        lv_obj_set_size(shuntPage.quadro_bars[i], 35, 90);
        lv_obj_align(shuntPage.quadro_bars[i], LV_ALIGN_CENTER, 0, 20);
        lv_bar_set_range(shuntPage.quadro_bars[i], 0, 30); // 30A max
        lv_bar_set_value(shuntPage.quadro_bars[i], 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(shuntPage.quadro_bars[i], lv_color_make(50, 50, 50), LV_PART_MAIN);
        lv_obj_set_style_bg_color(shuntPage.quadro_bars[i], shunt_colors[i + 1], LV_PART_INDICATOR);

        // Dikdörtgen şekil için radius'u küçük yapın veya sıfırlayın
        lv_obj_set_style_radius(shuntPage.quadro_bars[i], 3, LV_PART_MAIN);        // Ana kısım için küçük radius
        lv_obj_set_style_radius(shuntPage.quadro_bars[i], 3, LV_PART_INDICATOR);   // Indicator için de aynı

        // Quadro Shunt value label
        shuntPage.quadro_value_labels[i] = lv_label_create(shuntPage.quadro_containers[i]);
        lv_label_set_text(shuntPage.quadro_value_labels[i], "0.0A\n0.0V\n0.0W");
        lv_obj_set_style_text_font(shuntPage.quadro_value_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(shuntPage.quadro_value_labels[i], color_secondary, 0);
        lv_obj_set_style_text_align(shuntPage.quadro_value_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(shuntPage.quadro_value_labels[i], LV_ALIGN_LEFT_MID, -10, 15);
    }

    // Create status label - daha yukarıda
    shuntPage.status_label = lv_label_create(shuntPage.page_container);
    lv_label_set_text(shuntPage.status_label, "Scanning for shunt sensors...");
    lv_obj_set_style_text_font(shuntPage.status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(shuntPage.status_label, color_primary, 0);
    lv_obj_align(shuntPage.status_label, LV_ALIGN_BOTTOM_MID, 55, -15); // 0 + 60 = 60

    // Enable requests
    shuntPage.getShuntRequest = true;

    // Initially hide the page
    usrShuntPage_hide();

    ESP_LOGI(s_tag, "Shunt page initialized for STM32 communication with voltage divider correction");
}

void usrShuntPage_show(void)
{
    if (shuntPage.page_container != NULL)
    {
        lv_obj_clear_flag(shuntPage.page_container, LV_OBJ_FLAG_HIDDEN);
        shuntPage.is_active = true;

        // Ayarları yeniden yükle ve uygula
        load_shunt_settings();
        //apply_shunt_visibility();
        update_shunt_labels();

        // Start data collection when page is shown
        usrShuntPage_startDataCollection();

        ESP_LOGI(s_tag, "Shunt page shown with updated settings");
    }
}

void usrShuntPage_hide(void)
{
    if (shuntPage.page_container != NULL)
    {
        lv_obj_add_flag(shuntPage.page_container, LV_OBJ_FLAG_HIDDEN);
        shuntPage.is_active = false;
        usrShuntPage_stopDataCollection();

        ESP_LOGI(s_tag, "Shunt page hidden");
    }
}

void usrShuntPage_destroy(void)
{
    if (shuntPage.page_container != NULL)
    {
        usrShuntPage_stopDataCollection();
        lv_obj_del(shuntPage.page_container);
        memset(&shuntPage, 0, sizeof(usrShuntPage_t));

        ESP_LOGI(s_tag, "Shunt page destroyed");
    }
}

void usrShuntPage_setStatus(const char *status)
{
    if (shuntPage.status_label != NULL && status != NULL)
    {
        lv_label_set_text(shuntPage.status_label, status);
    }
}

// Process received CAN data - STM32'den gelen yanıtları işle
void usrShuntPage_processCanData(uint32_t can_id, uint32_t data)
{
    // Check Quadro Shunt responses (eğer varsa)
    if (can_id >= QUADRO_SHUNT_RESPONSE_ID_BASE && can_id <= (QUADRO_SHUNT_RESPONSE_ID_BASE + 3))
    {
        int quadro_index = can_id - QUADRO_SHUNT_RESPONSE_ID_BASE;

        // Extract current and voltage
        float current = (float)(data & 0xFFFF) / 1000.0f;         // mA to A
        float voltage = (float)((data >> 16) & 0xFFFF) / 1000.0f; // mV to V

        usrShuntPage_updateShuntData(quadro_index, SHUNT_TYPE_QUADRO, current, voltage);

        ESP_LOGI(s_tag, "Quadro Shunt %d: %.2fA, %.2fV", quadro_index + 1, current, voltage);
    }
    // STM32'den Main Shunt response (0x280)
    else if (can_id == MAIN_SHUNT_RESPONSE_ID)
    {
        // STM32'den gelen veri formatı:
        // İlk paket: [CMD][TYPE][Main_Bat_H][Main_Bat_L][Backup_Bat_H][Backup_Bat_L][Shunt_H][Shunt_L]
        // Bu durumda data 32-bit packed format olacak

        // Veriyi parse et (STM32'nin gönderdiği format)
        uint16_t main_battery_adc = (data >> 16) & 0xFFFF; // Main Battery ADC
        uint16_t backup_battery_adc = data & 0xFFFF;       // Backup Battery ADC

        // Shunt akımı için ayrı bir mesaj gelebilir veya bu mesajda olabilir
        // Şimdilik sadece voltaj verilerini işleyelim

        // ADC değerlerini gerçek voltajlara çevir
        float main_battery_voltage = convert_adc_to_voltage(main_battery_adc);
        float backup_battery_voltage = convert_adc_to_voltage(backup_battery_adc);

        // Batarya verilerini güncelle
        usrShuntPage_updateBatteryData(0, main_battery_voltage);   // Ana batarya
        usrShuntPage_updateBatteryData(1, backup_battery_voltage); // Yedek batarya

        // Main shunt için örnek akım verisi (gerçekte ayrı bir ADC'den gelecek)
        // Bu kısım STM32'den shunt ADC verisi geldiğinde güncellenecek
        float shunt_current = 0.0f; // Şimdilik 0, STM32'den shunt verisi geldiğinde güncellenecek

        ESP_LOGI(s_tag, "STM32 Data - Main Battery: %.2fV, Backup Battery: %.2fV",
                 main_battery_voltage, backup_battery_voltage);
    }
}

// STM32'den gelen ham CAN verilerini işle (8-byte format)
void usrShuntPage_processCanDataExtended(uint32_t can_id, uint8_t *can_data, uint8_t data_length)
{
    if (can_id == MAIN_SHUNT_RESPONSE_ID && data_length >= 8)
    {
        // STM32'nin gönderdiği format:
        // [0]: RESPONSE_CMD (0x02)
        // [1]: DATA_TYPE_ALL_MEASUREMENTS (0x05)
        // [2-3]: Main Battery voltage (16-bit)
        // [4-5]: Backup Battery voltage (16-bit)
        // [6-7]: Main Shunt current (16-bit)

        if (can_data[0] == 0x02 && can_data[1] == 0x05) // Response command ve All measurements
        {
            // 16-bit değerleri birleştir
            uint16_t main_battery_adc = (can_data[2] << 8) | can_data[3];
            uint16_t backup_battery_adc = (can_data[4] << 8) | can_data[5];
            uint16_t shunt_current_adc = (can_data[6] << 8) | can_data[7];

            // ADC değerlerini gerçek değerlere çevir
            float main_battery_voltage = convert_adc_to_voltage(main_battery_adc);
            float backup_battery_voltage = convert_adc_to_voltage(backup_battery_adc);
            float shunt_current = convert_adc_to_current(shunt_current_adc);

            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

            // Main Shunt verilerini güncelle
            shuntPage.main_shunt.current = shunt_current;
            shuntPage.main_shunt.voltage = main_battery_voltage; // Ana batarya voltajını referans al
            shuntPage.main_shunt.power = calculate_power(shunt_current, main_battery_voltage);
            shuntPage.main_shunt.is_connected = true;
            shuntPage.main_shunt.last_data_time = current_time;

            // Batarya verilerini güncelle
            usrShuntPage_updateBatteryData(0, main_battery_voltage);   // Ana batarya
            usrShuntPage_updateBatteryData(1, backup_battery_voltage); // Yedek batarya

            // Display'leri güncelle
            usrShuntPage_updateMainShuntDisplay();

            ESP_LOGI(s_tag, "STM32 Complete Data - Main Bat: %.2fV, Backup Bat: %.2fV, Shunt: %.2fA",
                     main_battery_voltage, backup_battery_voltage, shunt_current);
        }
    }
}

// Update shunt data and display
void usrShuntPage_updateShuntData(int shunt_index, shunt_type_t shunt_type, float current, float voltage)
{
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (shunt_type == SHUNT_TYPE_MAIN)
    {
        // Update Main Shunt data
        shuntPage.main_shunt.current = current;
        shuntPage.main_shunt.voltage = voltage;
        shuntPage.main_shunt.power = calculate_power(current, voltage);
        shuntPage.main_shunt.is_connected = true;
        shuntPage.main_shunt.last_data_time = current_time;

        // Update Main Shunt display
        usrShuntPage_updateMainShuntDisplay();
    }
    else if (shunt_type == SHUNT_TYPE_QUADRO && shunt_index >= 0 && shunt_index < 4)
    {
        // Update Quadro Shunt data
        shuntPage.quadro_shunts[shunt_index].current = current;
        shuntPage.quadro_shunts[shunt_index].voltage = voltage;
        shuntPage.quadro_shunts[shunt_index].power = calculate_power(current, voltage);
        shuntPage.quadro_shunts[shunt_index].is_connected = true;
        shuntPage.quadro_shunts[shunt_index].last_data_time = current_time;

        // Update Quadro Shunt display
        usrShuntPage_updateQuadroShuntDisplay(shunt_index);
    }
}

// Update battery data and display
void usrShuntPage_updateBatteryData(int battery_index, float voltage)
{
    if (battery_index < 0 || battery_index >= 2)
        return;

    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Update battery data
    shuntPage.batteries[battery_index].voltage = voltage;
    shuntPage.batteries[battery_index].is_connected = true;
    shuntPage.batteries[battery_index].last_data_time = current_time;

    // Update Battery display
    usrShuntPage_updateBatteryDisplay(battery_index);
}

// Update Battery display
void usrShuntPage_updateBatteryDisplay(int battery_index)
{
    if (battery_index < 0 || battery_index >= 2)
        return;
    if (shuntPage.battery_arcs[battery_index] == NULL)
        return;

    shunt_battery_data_t *battery = &shuntPage.batteries[battery_index];

    // Calculate battery percentage based on 12V lead-acid battery curve
    float percentage = ((battery->voltage - battery->min_voltage) /
                        (battery->max_voltage - battery->min_voltage)) *
                       100.0f;

    // Clamp percentage between 0-100
    if (percentage > 100.0f)
        percentage = 100.0f;
    if (percentage < 0.0f)
        percentage = 0.0f;

    // Limit update frequency
    static uint32_t last_battery_update[2] = {0, 0};
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (current_time - last_battery_update[battery_index] > 200)
    { // Limit updates to 5Hz
        // Update arc value
        lv_arc_set_value(shuntPage.battery_arcs[battery_index], (int)percentage);

        // Update voltage label
        char voltage_text[10];
        snprintf(voltage_text, sizeof(voltage_text), "%.1fV", battery->voltage);
        lv_label_set_text(shuntPage.battery_voltage_labels[battery_index], voltage_text);

        // Update percentage label
        char percentage_text[10];
        snprintf(percentage_text, sizeof(percentage_text), "%.0f%%", percentage);
        lv_label_set_text(shuntPage.battery_percentage_labels[battery_index], percentage_text);

        // Change arc color based on voltage level
        lv_color_t color = get_battery_color(battery->voltage, battery->min_voltage, battery->max_voltage);
        lv_obj_set_style_arc_color(shuntPage.battery_arcs[battery_index], color, LV_PART_INDICATOR);

        last_battery_update[battery_index] = current_time;
    }
}

// Update Main Shunt display
void usrShuntPage_updateMainShuntDisplay(void)
{
    if (shuntPage.main_shunt_bar == NULL)
        return;

    shunt_data_t *main = &shuntPage.main_shunt;

    // Limit update frequency to prevent rendering issues
    static uint32_t last_main_update = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (current_time - last_main_update > 100)
    { // Limit updates to 10Hz
        // Update bar value (mutlak değer kullan, negatif akımlar için)
        float abs_current = (main->current < 0) ? -main->current : main->current;
        lv_bar_set_value(shuntPage.main_shunt_bar, (int)abs_current, LV_ANIM_OFF);

        // Update value label - akım yönünü göster
        char value_text[80];
        const char *direction = (main->current >= 0) ? "CHARGE" : "DISCHARGE";
        snprintf(value_text, sizeof(value_text), "%.1fA %s | %.1fV | %.1fW",
                 abs_current, direction, main->voltage, main->power);
        lv_label_set_text(shuntPage.main_shunt_value_label, value_text);

        // Change bar color based on current level
        lv_color_t color = get_current_color(abs_current, main->max_current);
        lv_obj_set_style_bg_color(shuntPage.main_shunt_bar, color, LV_PART_INDICATOR);

        // Update connection status
        if (main->is_connected)
        {
            usrShuntPage_setStatus("STM32 Connected - Monitoring battery voltages and shunt current");
        }

        last_main_update = current_time;
    }
}

// Update Quadro Shunt display
void usrShuntPage_updateQuadroShuntDisplay(int quadro_index)
{
    if (quadro_index < 0 || quadro_index >= 4)
        return;
    if (shuntPage.quadro_bars[quadro_index] == NULL)
        return;

    shunt_data_t *quadro = &shuntPage.quadro_shunts[quadro_index];

    // Prevent too frequent updates
    static uint32_t last_update[4] = {0, 0, 0, 0};
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (current_time - last_update[quadro_index] > 100)
    { // Limit updates to 10Hz
        // Update bar value
        float abs_current = (quadro->current < 0) ? -quadro->current : quadro->current;
        lv_bar_set_value(shuntPage.quadro_bars[quadro_index], (int)abs_current, LV_ANIM_OFF);

        // Update value label
        char value_text[40];
        snprintf(value_text, sizeof(value_text), "%.1fA\n%.1fV\n%.1fW",
                 quadro->current, quadro->voltage, quadro->power);
        lv_label_set_text(shuntPage.quadro_value_labels[quadro_index], value_text);

        // Change bar color based on current level
        lv_color_t color = get_current_color(abs_current, quadro->max_current);
        lv_obj_set_style_bg_color(shuntPage.quadro_bars[quadro_index], color, LV_PART_INDICATOR);

        last_update[quadro_index] = current_time;
    }
}

void usrShuntPage_startDataCollection(void)
{
    if (shuntPage.update_timer == NULL)
    {
        // Start 2000ms timer for requesting shunt data
        shuntPage.update_timer = lv_timer_create(shunt_request_timer_cb, 2000, NULL);
        ESP_LOGI(s_tag, "Shunt data collection started (2000ms interval)");
        
        if (TEST_MODE) {
            usrShuntPage_setStatus("TEST MODE - Animated shunt data simulation running");
        } else {
            usrShuntPage_setStatus("Connecting to STM32 - Requesting battery and shunt data...");
        }
    }
}

void usrShuntPage_stopDataCollection(void)
{
    if (shuntPage.update_timer != NULL)
    {
        lv_timer_del(shuntPage.update_timer);
        shuntPage.update_timer = NULL;
        ESP_LOGI(s_tag, "Shunt data collection stopped");
        usrShuntPage_setStatus("Data collection stopped");
    }
}

bool usrShuntPage_isActive(void)
{
    return shuntPage.is_active;
}