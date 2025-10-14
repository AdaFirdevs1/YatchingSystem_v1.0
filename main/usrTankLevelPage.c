#include "usrTankLevelPage.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include "usrTankSettingsPage.h"
#include "usrTankPasswordPage.h"  // YENİ: Şifre doğrulama sayfası
#include "usrDefinePassword.h"     // YENİ: Şifre fonksiyonları için
#include <math.h>

// Font dosyanızı include edin - isim font dosyanızın içindeki declaration'a uymalı
extern lv_font_t lv_font_montserrat_10_omega;  // Font dosyanızdaki isim

static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Ana kurumsal renk (turkuaz)
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Açık arka plan
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Koyu yazı/kenarlık
static const lv_color_t color_background = LV_COLOR_MAKE(10, 10, 10);     // Ana arka plan
//static const lv_color_t color_accent = LV_COLOR_MAKE(216, 216, 216);      // Vurgu grisi
//static const lv_color_t color_inactive = LV_COLOR_MAKE(100, 100, 100);    // Pasif durum


static const char *s_tag = "usrTankLevelPage";
static usrTankLevelPage_t tankPage = {0};

// Test modu - tüm tank'ları görüntülemek için
static bool TEST_MODE = true;  // Test için true, normal kullanım için false


#define TANK_0_190_REQUEST_ID_BASE 0x320
#define TANK_0_190_RESPONSE_ID_BASE 0x320
#define TANK_30_240_REQUEST_ID_BASE 0x330
#define TANK_30_240_RESPONSE_ID_BASE 0x330

#define NVS_NAMESPACE "tank_config"
#define NVS_KEY_TANK_CONFIG "tank_cfg"

// Tank renkleri - kurumsal tema ile uyumlu
static const lv_color_t tank_colors[8] = {
    LV_COLOR_MAKE(146, 193, 193),   // Ana turkuaz
    LV_COLOR_MAKE(100, 180, 220),   // Açık mavi
    LV_COLOR_MAKE(180, 220, 180),   // Açık yeşil
    LV_COLOR_MAKE(220, 180, 120),   // Altın sarısı
    LV_COLOR_MAKE(200, 160, 200),   // Açık mor
    LV_COLOR_MAKE(220, 200, 160),   // Krem
    LV_COLOR_MAKE(160, 200, 220),   // Açık turkuaz
    LV_COLOR_MAKE(200, 220, 160)    // Açık lime
};

// Forward declarations
static void tank_config_click_cb(lv_event_t *e);
static void keyboard_event_cb(lv_event_t *e);
static void ta_ready_cb(lv_event_t *e);
static void dropdown_event_cb(lv_event_t *e);
static void create_config_popup(void);
static void close_config_popup(void);
static void modal_bg_click_cb(lv_event_t *e);
static void close_popup_timer_cb(lv_timer_t *timer);
static esp_err_t save_tank_config_to_nvs(void);
static esp_err_t load_tank_config_from_nvs(void);
static void settings_button_cb(lv_event_t *e);
static void back_from_settings_cb(void);
static void load_tank_names_from_settings_nvs(void);
// YENİ: Şifre doğrulama callback'leri
static void password_success_cb(void);
static void password_cancel_cb(void);

static esp_err_t save_tank_config_to_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    // NVS'yi aç
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    // Sadece konfigürasyon verilerini kaydet (8 tank için)
    typedef struct {
        uint16_t tank_capacity;
        sensor_type_t sensor_type;
        bool is_configured;
        // DÜZELTME: Tank isimlerini burada kaydetme, Settings'te zaten kaydediliyor
        // char tank_name[16]; // Bu satırı kaldır
    } tank_config_nvs_t;

    tank_config_nvs_t tank_configs[8];
    
    // Mevcut konfigürasyonları kopyala - SADECE TEKNİK VERİLER
    for (int i = 0; i < 8; i++) {
        tank_configs[i].tank_capacity = tankPage.tank_data[i].tank_capacity;
        tank_configs[i].sensor_type = tankPage.tank_data[i].sensor_type;
        tank_configs[i].is_configured = tankPage.tank_data[i].is_configured;
        // İsim kopyalamayı kaldır
    }

    // NVS'ye kaydet
    ret = nvs_set_blob(nvs_handle, NVS_KEY_TANK_CONFIG, tank_configs, sizeof(tank_configs));
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error saving tank config to NVS: %s", esp_err_to_name(ret));
    } else {
        // Değişiklikleri commit et
        ret = nvs_commit(nvs_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(s_tag, "Error committing NVS: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(s_tag, "Tank configurations saved to NVS successfully (without names)");
        }
    }

    // NVS handle'ını kapat
    nvs_close(nvs_handle);
    return ret;
}

static esp_err_t load_tank_config_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    // NVS'yi aç
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(s_tag, "Error opening NVS handle for reading: %s", esp_err_to_name(ret));
        return ret;
    }

    // DÜZELTME: Tank isimlerini içermeyen struct tanımı
    typedef struct {
        uint16_t tank_capacity;
        sensor_type_t sensor_type;
        bool is_configured;
        // İsim alanını kaldır
    } tank_config_nvs_t;

    tank_config_nvs_t tank_configs[8];
    size_t required_size = sizeof(tank_configs);

    // NVS'den oku - eski format uyumluluğu için size kontrolü
    ret = nvs_get_blob(nvs_handle, NVS_KEY_TANK_CONFIG, tank_configs, &required_size);
    if (ret == ESP_OK && required_size >= sizeof(tank_configs)) {
        // Konfigürasyonları geri yükle - SADECE TEKNİK VERİLER
        for (int i = 0; i < 8; i++) {
            tankPage.tank_data[i].tank_capacity = tank_configs[i].tank_capacity;
            tankPage.tank_data[i].sensor_type = tank_configs[i].sensor_type;
            tankPage.tank_data[i].is_configured = tank_configs[i].is_configured;
            // İsim yüklemeyi kaldır - mevcut isimler korunur
        }
        ESP_LOGI(s_tag, "Tank configurations loaded from NVS successfully (names preserved)");
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(s_tag, "No saved tank configuration found in NVS - using defaults");
    } else {
        ESP_LOGE(s_tag, "Error reading tank config from NVS: %s", esp_err_to_name(ret));
    }

    // NVS handle'ını kapat
    nvs_close(nvs_handle);
    return ret;
}


static float adc_to_level_percentage(uint16_t adc_value, sensor_type_t sensor_type)
{
    if (adc_value >= 4095)
        return 0.0f;

    float percentage = 0.0f;
    const float adc_min = 0.0f;
    const float adc_max = 275.0f;

    if (adc_value >= adc_max)
    {
        percentage = 100.0f;
    }
    else if (adc_value <= adc_min)
    {
        percentage = 0.0f;
    }
    else
    {
        percentage = ((float)adc_value / adc_max) * 100.0f;
    }

    return percentage;
}

// Timer callback for requesting tank data
static void tank_request_timer_cb(lv_timer_t *timer)
{
    if (tankPage.getTankRequest)
    {
        if (!TEST_MODE) 
        {
            // NORMAL MOD - Gerçek sensör kontrolü
            // Check for sensor timeouts (no data for 3 seconds)
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            for (int i = 0; i < 8; i++)
            {
                if (tankPage.tank_data[i].is_connected)
                {
                    if (current_time - tankPage.tank_data[i].last_data_time > 3000)
                    {
                        tankPage.tank_data[i].is_connected = false;
                        usrTankLevelPage_destroyTankWidget(i);
                        ESP_LOGI(s_tag, "Tank %d sensor timeout - widget destroyed", i + 1);
                    }
                }
            }

            // Send requests for 0-190 ohm sensors
            for (int i = 0; i < 4; i++)
            {
                uint32_t request_id_0_190 = TANK_0_190_REQUEST_ID_BASE + i;
                esp_err_t result1 = sendCanHeader(request_id_0_190, 0);

                if (result1 == ESP_OK)
                {
                    ESP_LOGI(s_tag, "0-190ohm Tank %d data requested (ID: 0x%lX)", i + 1, request_id_0_190);
                }
                else
                {
                    ESP_LOGE(s_tag, "Failed to request 0-190ohm Tank %d data", i + 1);
                }
                vTaskDelay(pdMS_TO_TICKS(200));
            }

            // Send requests for 30-240 ohm sensors
            for (int i = 0; i < 4; i++)
            {
                uint32_t request_id_30_240 = TANK_30_240_REQUEST_ID_BASE + i;
                esp_err_t result2 = sendCanHeader(request_id_30_240, 0);

                if (result2 == ESP_OK)
                {
                    ESP_LOGI(s_tag, "30-240ohm Tank %d data requested (ID: 0x%lX)", i + 1, request_id_30_240);
                }
                else
                {
                    ESP_LOGE(s_tag, "Failed to request 30-240ohm Tank %d data", i + 1);
                }
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        } 
        else 
        {
            // TEST MODU - Sahte animasyonlu veriler
            static uint32_t test_counter = 0;
            test_counter++;
            
            for (int i = 0; i < 8; i++) 
            {
                if (tankPage.tank_data[i].is_connected) 
                {
                    // Her tank için farklı sinüs dalgası - dinamik animasyon
                    float base_level = 50.0f; // Orta seviye
                    float amplitude = 30.0f;  // Salınım genliği
                    float frequency = 0.05f;  // Yavaş animasyon
                    float phase = i * 1.0f;   // Her tank farklı faz
                    
                    float new_level = base_level + amplitude * sin((test_counter * frequency) + phase);
                    
                    // Sınırları kontrol et
                    if (new_level < 5.0f) new_level = 5.0f;
                    if (new_level > 95.0f) new_level = 95.0f;
                    
                    // Test ADC değeri hesapla (level'a göre)
                    uint16_t test_adc = (uint16_t)(new_level * 275.0f / 100.0f);
                    
                    // Tank verilerini güncelle
                    tankPage.tank_data[i].level_percentage = new_level;
                    tankPage.tank_data[i].adc_value = test_adc;
                    tankPage.tank_data[i].last_data_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    
                    // Hacim hesapla (eğer konfigüre edilmişse)
                    if (tankPage.tank_data[i].is_configured) 
                    {
                        tankPage.tank_data[i].volume_liters = 
                            (new_level / 100.0f) * tankPage.tank_data[i].tank_capacity;
                    }
                    else
                    {
                        tankPage.tank_data[i].volume_liters = 0.0f;
                    }
                    
                    // Display'i güncelle
                    usrTankLevelPage_updateTankDisplay(i);
                }
            }
            
            ESP_LOGI(s_tag, "[TEST MODE] All tanks updated with animated test data (counter: %ld)", test_counter);
        }
    }
}


// Create tank widget for connected sensor - SCROLLABLE VERSION
void usrTankLevelPage_createTankWidget(int tank_index)
{
    if (tank_index < 0 || tank_index >= 8)
        return;
    if (tankPage.tank_containers[tank_index] != NULL)
        return; // Already created

    // 4x2 grid layout - 4 columns, 2 rows
    int col = tank_index % 4;  // 4 sütun (0,1,2,3)
    int row = tank_index / 4;  // 2 satır (0,1)
    
    // Widget boyutları - görseldeki oranları koruyarak
    int widget_width = 160;   
    int widget_height = 250;  
    int spacing_x = 30;       
    int spacing_y = 45;       
    
    // Grid hesaplaması
    int total_grid_width = (4 * widget_width) + (3 * spacing_x);
    int start_x = (LV_HOR_RES - total_grid_width) / 2 + 5;
    int start_y = 80;

    int x = start_x + col * (widget_width + spacing_x);
    int y = start_y + row * (widget_height + spacing_y);

    // Create main container
    tankPage.tank_containers[tank_index] = lv_obj_create(tankPage.page_container);
    lv_obj_set_size(tankPage.tank_containers[tank_index], widget_width, widget_height);
    lv_obj_set_pos(tankPage.tank_containers[tank_index], x, y + 20);
    lv_obj_set_style_bg_color(tankPage.tank_containers[tank_index], color_background, 0);  
    lv_obj_set_style_border_width(tankPage.tank_containers[tank_index], 0, 0);
    lv_obj_set_style_pad_all(tankPage.tank_containers[tank_index], 12, 0);
    lv_obj_set_style_radius(tankPage.tank_containers[tank_index], 8, 0);
    lv_obj_clear_flag(tankPage.tank_containers[tank_index], LV_OBJ_FLAG_SCROLLABLE);

    // Tank name - en üstte, merkezi
    tankPage.tank_labels[tank_index] = lv_label_create(tankPage.tank_containers[tank_index]);
    lv_label_set_text_fmt(tankPage.tank_labels[tank_index], "%s", tankPage.tank_data[tank_index].tank_name);
    lv_obj_set_style_text_font(tankPage.tank_labels[tank_index], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(tankPage.tank_labels[tank_index], lv_color_white(), 0);
    lv_obj_set_style_text_align(tankPage.tank_labels[tank_index], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tankPage.tank_labels[tank_index], LV_ALIGN_TOP_MID, 0, 0);

    // Top horizontal line - görseldeki gibi üst çizgi
    lv_obj_t *top_line = lv_obj_create(tankPage.tank_containers[tank_index]);
    lv_obj_set_size(top_line, 120, 3);  // İnce çizgi
    lv_obj_set_pos(top_line, (widget_width - 120) / 2, 25);
    lv_obj_set_style_bg_color(top_line, tank_colors[tank_index], 0);  // Sizin tank renginiz
    lv_obj_set_style_border_width(top_line, 0, 0);
    lv_obj_set_style_radius(top_line, 0, 0);
    lv_obj_clear_flag(top_line, LV_OBJ_FLAG_SCROLLABLE);

    // Tank main area - dikdörtgen tank alanı
    lv_obj_t *tank_area = lv_obj_create(tankPage.tank_containers[tank_index]);
    lv_obj_set_size(tank_area, 120, 150);  // Kare tank
    lv_obj_set_pos(tank_area, (widget_width - 120) / 2, 28);  // Top line'ın hemen altında
    lv_obj_set_style_bg_color(tank_area, lv_color_make(20, 20, 20), 0);  // Çok koyu arka plan
    lv_obj_set_style_border_width(tank_area, 0, 0);
    lv_obj_set_style_radius(tank_area, 0, 0);  // Keskin köşeler
    lv_obj_clear_flag(tank_area, LV_OBJ_FLAG_SCROLLABLE);


    // Liquid level bar - alttan yukarı dolan
    tankPage.tank_charts[tank_index] = lv_bar_create(tank_area);
    lv_obj_set_size(tankPage.tank_charts[tank_index], 120, 150);  // Tank area ile aynı boyut
    lv_obj_set_pos(tankPage.tank_charts[tank_index], -20, -20);      // Tank area'nın tam üzerine
    lv_bar_set_range(tankPage.tank_charts[tank_index], 0, 100);
    lv_bar_set_value(tankPage.tank_charts[tank_index], 0, LV_ANIM_OFF);


    // Bar styling
    lv_obj_set_style_bg_color(tankPage.tank_charts[tank_index], lv_color_make(35, 35, 35), LV_PART_MAIN); // Boş alan gri
    lv_obj_set_style_border_width(tankPage.tank_charts[tank_index], 0, LV_PART_MAIN);
    lv_obj_set_style_radius(tankPage.tank_charts[tank_index], 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tankPage.tank_charts[tank_index], 0, LV_PART_MAIN);  // YENİ: Padding sıfır

    // Indicator (dolu kısım) - sizin renklerinizi kullanıyor
    lv_obj_set_style_bg_color(tankPage.tank_charts[tank_index], tank_colors[tank_index], LV_PART_INDICATOR);
    lv_obj_set_style_radius(tankPage.tank_charts[tank_index], 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(tankPage.tank_charts[tank_index], 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(tankPage.tank_charts[tank_index], 0, LV_PART_INDICATOR);  // YENİ: Indicator padding sıfır

    // Percentage label - tank ortasında BÜYÜK ve NET
    tankPage.tank_value_labels[tank_index] = lv_label_create(tank_area);
    lv_label_set_text(tankPage.tank_value_labels[tank_index], "65\n%");
    lv_obj_set_style_text_font(tankPage.tank_value_labels[tank_index], &lv_font_montserrat_24, 0);  // Büyük font
    lv_obj_set_style_text_color(tankPage.tank_value_labels[tank_index], lv_color_white(), 0);  // Daima beyaz
    lv_obj_set_style_text_align(tankPage.tank_value_labels[tank_index], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(tankPage.tank_value_labels[tank_index]);

    // Bottom horizontal line - görseldeki gibi alt çizgi
    lv_obj_t *bottom_line = lv_obj_create(tankPage.tank_containers[tank_index]);
    lv_obj_set_size(bottom_line, 120, 3);  // İnce çizgi
    lv_obj_set_pos(bottom_line, (widget_width - 120) / 2, 184);  // Tank alanının hemen altında
    lv_obj_set_style_bg_color(bottom_line, tank_colors[tank_index], 0);  // Sizin tank renginiz
    lv_obj_set_style_border_width(bottom_line, 0, 0);
    lv_obj_set_style_radius(bottom_line, 0, 0);
    lv_obj_clear_flag(bottom_line, LV_OBJ_FLAG_SCROLLABLE);

    // Litre info - tank altında BÜYÜK font ile görseldeki gibi
    tankPage.tank_status_labels[tank_index] = lv_label_create(tankPage.tank_containers[tank_index]);
    lv_label_set_text(tankPage.tank_status_labels[tank_index], "0L\nClick Config");  // Küçük 'l' harfi görseldeki gibi
    lv_obj_set_style_text_font(tankPage.tank_status_labels[tank_index], &lv_font_montserrat_16, 0);  // Büyük font
    lv_obj_set_style_text_color(tankPage.tank_status_labels[tank_index], lv_color_white(), 0);  // Beyaz yazı
    lv_obj_set_style_text_align(tankPage.tank_status_labels[tank_index], LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(tankPage.tank_status_labels[tank_index], 5, 200);
    lv_obj_set_width(tankPage.tank_status_labels[tank_index], widget_width - 20);

    // Make entire container clickable for config
    lv_obj_add_flag(tankPage.tank_containers[tank_index], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tankPage.tank_containers[tank_index], tank_config_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)tank_index);

    ESP_LOGI(s_tag, "Tank %d widget created with exact image-matching design at position (%d, %d)", tank_index + 1, x, y);
}
// Destroy tank widget for disconnected sensor
void usrTankLevelPage_destroyTankWidget(int tank_index)
{
    if (tank_index < 0 || tank_index >= 8)
        return;
    if (tankPage.tank_containers[tank_index] == NULL)
        return; // Already destroyed

    // Delete the container (this deletes all child objects)
    lv_obj_del(tankPage.tank_containers[tank_index]);

    // Reset pointers
    tankPage.tank_containers[tank_index] = NULL;
    tankPage.tank_charts[tank_index] = NULL;
    tankPage.tank_labels[tank_index] = NULL;
    tankPage.tank_value_labels[tank_index] = NULL;
    tankPage.tank_status_labels[tank_index] = NULL;

    ESP_LOGI(s_tag, "Tank %d widget destroyed", tank_index + 1);
}

// Update tank display with current data - İYİLEŞTİRİLMİŞ
void usrTankLevelPage_updateTankDisplay(int tank_index)
{
    if (tank_index < 0 || tank_index >= 8)
        return;
    if (tankPage.tank_charts[tank_index] == NULL)
        return;

    tank_sensor_data_t *tank = &tankPage.tank_data[tank_index];

    // Update bar value with smooth animation
    lv_bar_set_value(tankPage.tank_charts[tank_index], (int)tank->level_percentage, LV_ANIM_ON);

    // Update percentage - görseldeki gibi format (büyük sayı + alt satırda %)
    char percentage_text[8];
    snprintf(percentage_text, sizeof(percentage_text), "%.0f\n%%", tank->level_percentage);
    lv_label_set_text(tankPage.tank_value_labels[tank_index], percentage_text);

    // Update litre info - görseldeki gibi sadece sayı + küçük 'l'
    char info_text[24];
    if (tank->is_configured)
    {
        snprintf(info_text, sizeof(info_text), "%.0fL\n%s",
                 tank->volume_liters,
                 tank->sensor_type == SENSOR_TYPE_0_190 ? "0-190 Ohm" : "30-240 Ohm");
    }
    else
    {
        snprintf(info_text, sizeof(info_text), "--L\nClick Config");
    }
    lv_label_set_text(tankPage.tank_status_labels[tank_index], info_text);

    // Update colors based on level - sizin renklerinizi koruyorum
    lv_color_t liquid_color = tank_colors[tank_index];  // Varsayılan sizin renginiz
    
    if (tank->level_percentage < 20.0f)
    {
        liquid_color = lv_palette_main(LV_PALETTE_RED);  // Düşük seviye kırmızı
    }
    else if (tank->level_percentage > 80.0f)
    {
        liquid_color = color_primary;  // Yüksek seviye turkuaz
    }
    // Aksi halde sizin tank renginizi kullan
    
    // Update indicator color
    lv_obj_set_style_bg_color(tankPage.tank_charts[tank_index], liquid_color, LV_PART_INDICATOR);
    
    // Text color daima beyaz (görseldeki gibi)
    lv_obj_set_style_text_color(tankPage.tank_value_labels[tank_index], lv_color_white(), 0);
}

// Tank configuration click callback
static void tank_config_click_cb(lv_event_t *e)
{
    int tank_index = (int)(intptr_t)lv_event_get_user_data(e);
    tankPage.current_tank_index = tank_index;
    usrTankLevelPage_showConfigPopup(tank_index);
}

// Create configuration popup - POPUP DÜZENI İYİLEŞTİRİLDİ
static void create_config_popup(void)
{
    // Stop requests when popup opens
    tankPage.getTankRequest = false;

    if (tankPage.modal_bg != NULL)
        return; // Already created

    // Create modal background - tam ekran overlay
    tankPage.modal_bg = lv_obj_create(lv_scr_act());  // Ana ekranda oluştur, scroll container'da değil
    lv_obj_set_size(tankPage.modal_bg, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(tankPage.modal_bg, 0, 0);
    lv_obj_set_style_bg_color(tankPage.modal_bg, color_background, 0);
    lv_obj_set_style_bg_opa(tankPage.modal_bg, LV_OPA_80, 0);
    lv_obj_set_style_border_width(tankPage.modal_bg, 0, 0);
    lv_obj_clear_flag(tankPage.modal_bg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(tankPage.modal_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tankPage.modal_bg, modal_bg_click_cb, LV_EVENT_CLICKED, NULL);

    // Create popup container - tam merkezi
    tankPage.input_popup = lv_obj_create(tankPage.modal_bg);
    lv_obj_set_size(tankPage.input_popup, 500, 320); 
    lv_obj_set_pos(tankPage.input_popup, 130, 140);
    lv_obj_set_style_bg_color(tankPage.input_popup, color_background, 0);
    lv_obj_set_style_border_color(tankPage.input_popup, color_primary, 0);  // Turkuaz border
    lv_obj_set_style_border_width(tankPage.input_popup, 3, 0);
    lv_obj_set_style_radius(tankPage.input_popup, 12, 0);
    lv_obj_clear_flag(tankPage.input_popup, LV_OBJ_FLAG_SCROLLABLE);

    // Create title
    lv_obj_t *title_label = lv_label_create(tankPage.input_popup);
    lv_label_set_text_fmt(title_label, "Configure %s", tankPage.tank_data[tankPage.current_tank_index].tank_name);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, color_primary, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // Create capacity label
    lv_obj_t *capacity_label = lv_label_create(tankPage.input_popup);
    lv_label_set_text(capacity_label, "Tank Capacity (Liters):");
    lv_obj_set_style_text_font(capacity_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(capacity_label, color_secondary, 0);
    lv_obj_set_pos(capacity_label, 60, 50);

    // Create capacity text area
    tankPage.ta_capacity = lv_textarea_create(tankPage.input_popup);
    lv_obj_set_size(tankPage.ta_capacity, 150, 40);
    lv_obj_set_pos(tankPage.ta_capacity, 60, 75);
    lv_textarea_set_placeholder_text(tankPage.ta_capacity, "Enter 1-9999L");
    lv_textarea_set_one_line(tankPage.ta_capacity, true);
    lv_textarea_set_max_length(tankPage.ta_capacity, 4);
    lv_obj_add_event_cb(tankPage.ta_capacity, keyboard_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(tankPage.ta_capacity, ta_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_set_style_border_color(tankPage.ta_capacity, color_primary, 0);
    lv_obj_set_style_border_width(tankPage.ta_capacity, 2, 0);
    lv_obj_set_style_bg_color(tankPage.ta_capacity, color_dark, 0);  // (30,30,30 yerine)
    lv_obj_set_style_text_color(tankPage.ta_capacity, color_secondary, 0);  // (220,220,220 yerine)
    lv_obj_set_style_radius(tankPage.ta_capacity, 6, 0);
    lv_obj_set_style_pad_all(tankPage.ta_capacity, 8, 0);

    // Create sensor type label
    lv_obj_t *sensor_label = lv_label_create(tankPage.input_popup);
    lv_label_set_text(sensor_label, "Sensor Type:");
    lv_obj_set_style_text_font(sensor_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sensor_label, color_secondary, 0);
    lv_obj_set_pos(sensor_label, 240, 50);

    // Create sensor type dropdown
    tankPage.sensor_type_dropdown = lv_dropdown_create(tankPage.input_popup);
    lv_dropdown_set_options(tankPage.sensor_type_dropdown, "0-190 Ohm\n30-240 Ohm");
    lv_obj_set_size(tankPage.sensor_type_dropdown, 150, 35);
    lv_obj_set_pos(tankPage.sensor_type_dropdown, 240, 75);
    lv_obj_add_event_cb(tankPage.sensor_type_dropdown, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_border_color(tankPage.sensor_type_dropdown, color_primary, 0);  // Mavi yerine turkuaz
    lv_obj_set_style_border_width(tankPage.sensor_type_dropdown, 2, 0);
    lv_obj_set_style_bg_color(tankPage.sensor_type_dropdown, color_dark, 0);
    lv_obj_set_style_text_color(tankPage.sensor_type_dropdown, color_secondary, 0);
    lv_obj_set_style_radius(tankPage.sensor_type_dropdown, 6, 0);
    lv_obj_set_style_pad_all(tankPage.sensor_type_dropdown, 8, 0);

    // Create keyboard - daha optimize boyut
    tankPage.keyboard = lv_keyboard_create(tankPage.input_popup);
    lv_keyboard_set_mode(tankPage.keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(tankPage.keyboard, 400, 140);  // Daha büyük keyboard
    lv_obj_align(tankPage.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(tankPage.keyboard, tankPage.ta_capacity);
    lv_obj_set_style_radius(tankPage.keyboard, 10, 0);     
    lv_obj_set_style_border_color(tankPage.keyboard, color_primary, 0);  // Turkuaz border
    lv_obj_set_style_bg_color(tankPage.keyboard, color_dark, 0);  // (20,20,20 yerine)
    lv_obj_set_style_border_width(tankPage.keyboard, 2, 0);

    // Set current values if configured
    if (tankPage.tank_data[tankPage.current_tank_index].is_configured)
    {
        char capacity_str[8];
        snprintf(capacity_str, sizeof(capacity_str), "%d", tankPage.tank_data[tankPage.current_tank_index].tank_capacity);
        lv_textarea_set_text(tankPage.ta_capacity, capacity_str);
        lv_dropdown_set_selected(tankPage.sensor_type_dropdown, tankPage.tank_data[tankPage.current_tank_index].sensor_type);
    }
}

// Close configuration popup
static void close_config_popup(void)
{
    if (tankPage.modal_bg != NULL)
    {
        // Event handler'ları temizle (güvenlik için)
        lv_obj_remove_event_cb(tankPage.modal_bg, modal_bg_click_cb);
        
        lv_obj_del(tankPage.modal_bg);
        tankPage.modal_bg = NULL;
        tankPage.input_popup = NULL;
        tankPage.keyboard = NULL;
        tankPage.ta_capacity = NULL;
        tankPage.sensor_type_dropdown = NULL;
    }
    // Resume requests when popup closes
    tankPage.getTankRequest = true;
    
    ESP_LOGI(s_tag, "Config popup closed safely");
}

static void modal_bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_obj_t *current_target = lv_event_get_current_target(e);
    
    // Sadece modal background'a tıklandığında kapat (popup container'a değil)
    if (target == current_target && target == tankPage.modal_bg)
    {
        ESP_LOGI(s_tag, "Modal background clicked - closing popup with timer");
        
        // Timer ile güvenli kapatma
        lv_timer_t *close_timer = lv_timer_create(close_popup_timer_cb, 10, NULL);
        lv_timer_set_repeat_count(close_timer, 1);
    }
}


static void close_popup_timer_cb(lv_timer_t *timer)
{
    // Timer'ı sil
    lv_timer_del(timer);
    
    // Popup'ı güvenli şekilde kapat
    close_config_popup();
}

// Keyboard event callback
static void keyboard_event_cb(lv_event_t *e)
{
    // Keyboard is already linked to text area
}

// Text area ready callback (Enter pressed)
static void ta_ready_cb(lv_event_t *e)
{
    // Get values and save configuration
    const char *capacity_text = lv_textarea_get_text(tankPage.ta_capacity);
    uint16_t capacity = atoi(capacity_text);
    uint16_t sensor_type = lv_dropdown_get_selected(tankPage.sensor_type_dropdown);

    if (capacity >= 1 && capacity <= 9999)
    {
        usrTankLevelPage_saveTankConfig(tankPage.current_tank_index, capacity, (sensor_type_t)sensor_type);
        
        // DEĞIŞIKLIK: Hemen kapatmak yerine 50ms sonra kapat
        lv_timer_t *close_timer = lv_timer_create(close_popup_timer_cb, 50, NULL);
        lv_timer_set_repeat_count(close_timer, 1);
        
        ESP_LOGI(s_tag, "Configuration saved, popup will close in 50ms");
    }
    else
    {
        ESP_LOGW(s_tag, "Invalid capacity: %d (range: 1-9999L)", capacity);
        // Geçersiz değer durumunda popup'ı kapatma
    }
}

// Dropdown event callback
static void dropdown_event_cb(lv_event_t *e)
{
    // Nothing to do here, value is read when saving
}

// GÜNCELLENMIŞ Settings button callback - ŞİFRE KONTROLÜ EKLENMIŞ
static void settings_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Settings button pressed");
    
    // Önce şifrenin tanımlanıp tanımlanmadığını kontrol et
    if (!usrDefinePassword_isPasswordDefined()) {
        ESP_LOGW(s_tag, "No password defined - direct access to settings");
        
        // Şifre yoksa direkt ayarlara git
        usrTankLevelPage_hide();
        usrTankSettingsPage_show();
    } else {
        ESP_LOGI(s_tag, "Password defined - showing password verification");
        
        // Şifre varsa önce şifre doğrulama sayfasını göster
        usrTankPasswordPage_show(password_success_cb, password_cancel_cb);
    }
}

// YENİ: Şifre doğrulama başarılı callback
static void password_success_cb(void)
{
    ESP_LOGI(s_tag, "Password verification successful - accessing settings");
    
    // Şifre doğru - ayarlara git
    usrTankLevelPage_hide();
    usrTankSettingsPage_show();
}

// YENİ: Şifre doğrulama iptal/başarısız callback
static void password_cancel_cb(void)
{
    ESP_LOGI(s_tag, "Password verification cancelled or failed");
    
    // Şifre yanlış veya iptal edildi - ana sayfada kal
    // Herhangi bir işlem yapma, sadece ana sayfada kal
}

// Back from settings callback - YENİ FONKSIYON
static void back_from_settings_cb(void)
{
    ESP_LOGI(s_tag, "Returning from settings page");
    
    // Hide settings page
    usrTankSettingsPage_hide();
    
    // Reload tank names from NVS
    char tank_names[8][16];
    usrTankSettingsPage_loadTankNames(tank_names);
    
    // Update tank names in tank data
    for (int i = 0; i < 8; i++) {
        strncpy(tankPage.tank_data[i].tank_name, tank_names[i], sizeof(tankPage.tank_data[i].tank_name));
        
        // Update widget label if it exists
        if (tankPage.tank_labels[i] != NULL) {
            lv_label_set_text(tankPage.tank_labels[i], tank_names[i]);
        }
    }
    
    // Show tank page again
    usrTankLevelPage_show();
}

// Load tank names from NVS - YENİ FONKSIYON
static void load_tank_names_from_settings_nvs(void)
{
    char tank_names[8][16];
    usrTankSettingsPage_loadTankNames(tank_names);
    
    // Update tank names in tank data
    for (int i = 0; i < 8; i++) {
        strncpy(tankPage.tank_data[i].tank_name, tank_names[i], sizeof(tankPage.tank_data[i].tank_name));
    }
    
    ESP_LOGI(s_tag, "Tank names loaded from settings NVS");
}



// Show configuration popup
void usrTankLevelPage_showConfigPopup(int tank_index)
{
    if (tank_index < 0 || tank_index >= 8)
        return;
    tankPage.current_tank_index = tank_index;
    create_config_popup();
}

// Save tank configuration
void usrTankLevelPage_saveTankConfig(int tank_index, uint16_t capacity, sensor_type_t sensor_type)
{
    if (tank_index < 0 || tank_index >= 8)
        return;

    // Mevcut konfigürasyonu güncelle
    tankPage.tank_data[tank_index].tank_capacity = capacity;
    tankPage.tank_data[tank_index].sensor_type = sensor_type;
    tankPage.tank_data[tank_index].is_configured = true;

    // NVS'ye kaydet
    esp_err_t ret = save_tank_config_to_nvs();
    if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "Tank %d configured and SAVED: %dL capacity, %s sensor",
                 tank_index + 1, capacity,
                 sensor_type == SENSOR_TYPE_0_190 ? "0-190 Ohm" : "30-240 Ohm");
    } else {
        ESP_LOGE(s_tag, "Tank %d configured but FAILED to save to NVS!", tank_index + 1);
    }

    // Mevcut verilerle yeniden hesapla
    usrTankLevelPage_updateTankData(tank_index, tankPage.tank_data[tank_index].adc_value);
}

// Process received CAN data
void usrTankLevelPage_processCanData(uint32_t can_id, uint32_t data)
{
    int tank_index = -1;
    sensor_type_t detected_sensor_type;

    if (can_id >= TANK_0_190_RESPONSE_ID_BASE && can_id <= (TANK_0_190_RESPONSE_ID_BASE + 3))
    {
        tank_index = can_id - TANK_0_190_RESPONSE_ID_BASE;
        detected_sensor_type = SENSOR_TYPE_0_190;
    }
    else if (can_id >= TANK_30_240_RESPONSE_ID_BASE && can_id <= (TANK_30_240_RESPONSE_ID_BASE + 3))
    {
        tank_index = (can_id - TANK_30_240_RESPONSE_ID_BASE) + 4;
        detected_sensor_type = SENSOR_TYPE_30_240;
    }

    if (tank_index >= 0 && tank_index < 8)
    {
        uint16_t adc_value = (uint16_t)(data & 0xFFF);

        if (!tankPage.tank_data[tank_index].is_configured)
        {
            tankPage.tank_data[tank_index].sensor_type = detected_sensor_type;
        }

        usrTankLevelPage_updateTankData(tank_index, adc_value);

        ESP_LOGI(s_tag, "Tank %d (%s): ADC=%d, Connected=%s",
                 tank_index + 1,
                 detected_sensor_type == SENSOR_TYPE_0_190 ? "0-190ohm" : "30-240ohm",
                 adc_value,
                 adc_value < 4095 ? "Yes" : "No");
    }
}

// Update tank data from ADC value
void usrTankLevelPage_updateTankData(int tank_index, uint16_t adc_value)
{
    if (tank_index < 0 || tank_index >= 8)
        return;

    tank_sensor_data_t *tank = &tankPage.tank_data[tank_index];
    bool was_connected = tank->is_connected;

    // STM sadece sensör bağlıysa (ADC < 3000) veri gönderiyor
    // Dolayısıyla veri geliyorsa sensör bağlıdır
    tank->is_connected = true;
    tank->adc_value = adc_value;
    tank->last_data_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Handle connection state changes
    if (tank->is_connected && !was_connected)
    {
        // Sensor just connected - create widget
        usrTankLevelPage_createTankWidget(tank_index);
    }

    if (tank->is_connected)
    {
        // Direct ADC to percentage mapping (0-620 ADC = 0-100%)
        tank->level_percentage = adc_to_level_percentage(adc_value, tank->sensor_type);

        // Calculate volume if configured
        if (tank->is_configured)
        {
            tank->volume_liters = (tank->level_percentage / 100.0f) * tank->tank_capacity;
        }
        else
        {
            tank->volume_liters = 0.0f;
        }

        // Update display
        usrTankLevelPage_updateTankDisplay(tank_index);
    }
}


// BAŞLANGIÇ FONKSİYONU
void usrTankLevelPage_init(lv_obj_t *parent)
{
    if (tankPage.page_container != NULL)
    {
        ESP_LOGW(s_tag, "Tank level page already initialized");
        return;
    }

    // NVS'yi başlat (eğer daha önce başlatılmamışsa)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // UI'yi oluştur
    tankPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(tankPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(tankPage.page_container, -60, 0);
    lv_obj_set_style_bg_color(tankPage.page_container, color_background, 0);  // (10,10,10) yerine
    lv_obj_set_style_border_width(tankPage.page_container, 0, 0);
    lv_obj_set_style_pad_all(tankPage.page_container, 0, 0);
    
    // Scrollable ayarları
    lv_obj_set_scroll_dir(tankPage.page_container, LV_DIR_VER);
    lv_obj_set_style_bg_color(tankPage.page_container, color_background, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(tankPage.page_container, 8, LV_PART_SCROLLBAR);
    lv_obj_set_scrollbar_mode(tankPage.page_container, LV_SCROLLBAR_MODE_AUTO);

    // Title container oluştur
    lv_obj_t *title_container = lv_obj_create(tankPage.page_container);
    lv_obj_set_size(title_container, LV_HOR_RES - 40, 60);
    lv_obj_set_pos(title_container, 20, 0);
    lv_obj_set_style_bg_color(title_container, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);

    // Settings button (sağ üst köşe)
    lv_obj_t *settings_button = lv_btn_create(title_container);
    lv_obj_set_size(settings_button, 40, 40);
    lv_obj_set_pos(settings_button, LV_HOR_RES - 110, 0);
    lv_obj_set_style_bg_color(settings_button, color_primary, 0); 
    lv_obj_set_style_radius(settings_button, 5, 0);
    lv_obj_add_event_cb(settings_button, settings_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *settings_label = lv_label_create(settings_button);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(settings_label, color_dark, 0);
    lv_obj_center(settings_label);

    tankPage.title_label = lv_label_create(title_container);
    if (TEST_MODE) {
        lv_label_set_text(tankPage.title_label, "TANK MONITOR [TEST MODE]");
    } else {
        lv_label_set_text(tankPage.title_label, "TANK MONITOR");
    }
    lv_obj_set_style_text_font(tankPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(tankPage.title_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(tankPage.title_label, LV_ALIGN_LEFT_MID, 220, 10);

    // Status container oluştur
    lv_obj_t *status_container = lv_obj_create(tankPage.page_container);
    lv_obj_set_size(status_container, LV_HOR_RES, 35);
    lv_obj_set_pos(status_container, 0, 750);
    lv_obj_set_style_bg_color(status_container, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_border_width(status_container, 0, 0);
    lv_obj_clear_flag(status_container, LV_OBJ_FLAG_SCROLLABLE);

    tankPage.status_label = lv_label_create(status_container);
    lv_label_set_text(tankPage.status_label, " ");
    lv_obj_set_style_text_font(tankPage.status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tankPage.status_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_obj_center(tankPage.status_label);

    // YENİ: Şifre doğrulama sayfasını initialize et
    usrTankPasswordPage_init(parent);
    
    // Settings page'i initialize et
    usrTankSettingsPage_init(parent);
    usrTankSettingsPage_setBackCallback(back_from_settings_cb);

    // Tank data'yı başlat - ÖNCE VARSAYILAN İSİMLERLE
    for (int i = 0; i < 8; i++)
    {
        memset(&tankPage.tank_data[i], 0, sizeof(tank_sensor_data_t));
        if (i < 4)
        {
            snprintf(tankPage.tank_data[i].tank_name, sizeof(tankPage.tank_data[i].tank_name), "0-190 Ohm T%d", i + 1);
            tankPage.tank_data[i].sensor_type = SENSOR_TYPE_0_190;
        }
        else
        {
            snprintf(tankPage.tank_data[i].tank_name, sizeof(tankPage.tank_data[i].tank_name), "30-240 Ohm T%d", i - 3);
            tankPage.tank_data[i].sensor_type = SENSOR_TYPE_30_240;
        }
        tankPage.tank_data[i].adc_value = 4096;

        // Widget pointerları NULL'a set et
        tankPage.tank_containers[i] = NULL;
        tankPage.tank_charts[i] = NULL;
        tankPage.tank_labels[i] = NULL;
        tankPage.tank_value_labels[i] = NULL;
        tankPage.tank_status_labels[i] = NULL;
    }

    // KRITIK DÜZELTME: Tank isimlerini Settings NVS'den yükle
    // Bu işlem tank data initialization'dan SONRA olmalı
    load_tank_names_from_settings_nvs();
    ESP_LOGI(s_tag, "Tank names loaded from Settings NVS after initialization");

    // KAYITLI KONFİGÜRASYONLARI YÜKLE (sadece kapasite ve sensör tipi)
    // Bu işlem tank isimlerini etkilemeyecek şekilde güncellendi
    load_tank_config_from_nvs();
    ESP_LOGI(s_tag, "Tank configurations loaded from NVS");

    // TEST MODU - Başlangıçta tüm tank'ları oluştur
    if (TEST_MODE) 
    {
        ESP_LOGI(s_tag, "TEST MODE ACTIVE - Creating all tank widgets with test data");
        
        for (int i = 0; i < 8; i++) 
        {
            // Test verileri ile tank'ı bağlı olarak işaretle
            tankPage.tank_data[i].is_connected = true;
            tankPage.tank_data[i].adc_value = 100 + (i * 25); // Farklı test ADC değerleri
            tankPage.tank_data[i].level_percentage = 20.0f + (i * 8.0f); // 20% ile 76% arası
            tankPage.tank_data[i].last_data_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Hacim hesapla (eğer konfigüre edilmişse)
            if (tankPage.tank_data[i].is_configured) 
            {
                tankPage.tank_data[i].volume_liters = 
                    (tankPage.tank_data[i].level_percentage / 100.0f) * tankPage.tank_data[i].tank_capacity;
            }
            
            // Widget oluştur ve güncelle
            usrTankLevelPage_createTankWidget(i);
            usrTankLevelPage_updateTankDisplay(i);
        }
        
        usrTankLevelPage_setStatus(" ");
    }
    else 
    {
        usrTankLevelPage_setStatus(" ");
    }

    // Popup pointerları başlat
    tankPage.modal_bg = NULL;
    tankPage.input_popup = NULL;
    tankPage.keyboard = NULL;
    tankPage.ta_capacity = NULL;
    tankPage.sensor_type_dropdown = NULL;
    tankPage.current_tank_index = -1;

    // Request'leri aktif et
    tankPage.getTankRequest = true;

    // Sayfayı başlangıçta gizle
    usrTankLevelPage_hide();

    ESP_LOGI(s_tag, "Tank level page initialized with password protection and corrected name loading order");
}

void usrTankLevelPage_clearAllConfig(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_erase_key(nvs_handle, NVS_KEY_TANK_CONFIG);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(s_tag, "All tank configurations cleared from NVS");
    }
}

void usrTankLevelPage_show(void)
{
    if (tankPage.page_container != NULL)
    {
        lv_obj_clear_flag(tankPage.page_container, LV_OBJ_FLAG_HIDDEN);
        tankPage.is_active = true;

        // Start data collection when page is shown
        usrTankLevelPage_startDataCollection();

        ESP_LOGI(s_tag, "Tank level page shown");
    }
}

void usrTankLevelPage_hide(void)
{
    if (tankPage.page_container != NULL)
    {
        lv_obj_add_flag(tankPage.page_container, LV_OBJ_FLAG_HIDDEN);
        tankPage.is_active = false;
        usrTankLevelPage_stopDataCollection();

        // Close popup if open
        close_config_popup();

        ESP_LOGI(s_tag, "Tank level page hidden");
    }
}

void usrTankLevelPage_destroy(void)
{
    if (tankPage.page_container != NULL)
    {
        usrTankLevelPage_stopDataCollection();
        close_config_popup(); // Close any open popup

        // Destroy all tank widgets
        for (int i = 0; i < 8; i++)
        {
            usrTankLevelPage_destroyTankWidget(i);
        }

        // YENİ: Şifre doğrulama sayfasını da destroy et
        usrTankPasswordPage_destroy();

        lv_obj_del(tankPage.page_container);
        memset(&tankPage, 0, sizeof(usrTankLevelPage_t));

        ESP_LOGI(s_tag, "Tank level page destroyed with password protection");
    }
}

void usrTankLevelPage_setStatus(const char *status)
{
    if (tankPage.status_label != NULL && status != NULL)
    {
        lv_label_set_text(tankPage.status_label, status);
    }
}

bool usrTankLevelPage_isActive(void)
{
    return tankPage.is_active;
}

void usrTankLevelPage_startDataCollection(void)
{
    if (tankPage.update_timer == NULL)
    {
        // Start 2000ms timer for requesting tank data
        tankPage.update_timer = lv_timer_create(tank_request_timer_cb, 2000, NULL);
        ESP_LOGI(s_tag, "Tank data collection started (2000ms interval)");
        usrTankLevelPage_setStatus(" ");
    }
}

void usrTankLevelPage_stopDataCollection(void)
{
    if (tankPage.update_timer != NULL)
    {
        lv_timer_del(tankPage.update_timer);
        tankPage.update_timer = NULL;
        ESP_LOGI(s_tag, "Tank data collection stopped");
        usrTankLevelPage_setStatus(" ");
    }
}