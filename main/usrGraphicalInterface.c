#include "usrGraphicalInterface.h"
#include "usrGeneralDefines.h"
#include "usrCAN.h"
#include "usrDefinePassword.h" 
#include <time.h>

static const char *s_tag = "usrGraphicalInterface";
static usrGraphicalInterface_t gui = {0};

// Splash screen için yeni değişkenler
static lv_obj_t *splash_screen = NULL;
static lv_obj_t *logo_img = NULL;
static lv_anim_t fade_anim;
static lv_timer_t *splash_timer = NULL;
static bool splash_active = false;
static int splash_phase = 0; // 0: fade in, 1: hold, 2: fade out, 3: show main

// Logo image declaration - marine_logo.c dosyanızdan
LV_IMG_DECLARE(marine_logo);

static void password_defined_callback(bool success);
static void splash_fade_anim_cb(void *obj, int32_t value);
static void splash_timer_cb(lv_timer_t *timer);
static void create_splash_screen(void);
static void destroy_splash_screen(void);

// MAIN sayfası kaldırıldı - PAGE_COUNT 4 oldu
static const char *page_names[PAGE_COUNT] = {
    "BUTTONS",
    "BATTERY", 
    "TANK LEVEL",
    "SHUNT"
};

// Splash screen fade animasyon callback'i
static void splash_fade_anim_cb(void *obj, int32_t value)
{
    if (splash_screen != NULL) {
        lv_obj_set_style_opa(splash_screen, value, 0);
    }
}

// Splash screen timer callback'i - DÜZELTİLDİ
static void splash_timer_cb(lv_timer_t *timer)
{
    if (!splash_active) return;

    switch (splash_phase) {
        case 0: // Fade in tamamlandı, biraz bekle
            splash_phase = 1;
            lv_timer_set_period(splash_timer, 1500); // 1.5 saniye bekle
            ESP_LOGI(s_tag, "Splash phase 1: holding logo");
            break;
            
        case 1: // Bekleme tamamlandı, fade out başlat
            splash_phase = 2;
            lv_anim_init(&fade_anim);
            lv_anim_set_var(&fade_anim, splash_screen);
            lv_anim_set_exec_cb(&fade_anim, splash_fade_anim_cb);
            lv_anim_set_values(&fade_anim, LV_OPA_COVER, LV_OPA_TRANSP);
            lv_anim_set_time(&fade_anim, 800);
            lv_anim_start(&fade_anim);
            lv_timer_set_period(splash_timer, 1000); // Animasyon bitene kadar bekle
            ESP_LOGI(s_tag, "Splash phase 2: fading out");
            break;
            
        case 2: // Fade out tamamlandı, splash screen'i kaldır ve ana arayüzü göster
            splash_phase = 3;
            destroy_splash_screen();
            splash_active = false;
            
            // Ana container'ı göster - ÖNEMLİ DÜZELTİLDİ
            if (gui.main_container) {
                lv_obj_clear_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
            }
            
            // Ana arayüzü göster
            if (!usrDefinePassword_isPasswordDefined()) {
                ESP_LOGI(s_tag, "Password not defined, showing password setup page");
                usrDefinePassword_show();
            } else {
                ESP_LOGI(s_tag, "Password already defined, showing button page");
                usrGraphicalInterface_showPage(PAGE_BUTTON_CONTROL);
            }
            
            // Timer'ı temizle
            if (splash_timer) {
                lv_timer_del(splash_timer);
                splash_timer = NULL;
            }
            ESP_LOGI(s_tag, "Splash sequence completed, main interface shown");
            break;
    }
}

// Splash screen oluşturma - DÜZELTİLDİ
static void create_splash_screen(void)
{
    ESP_LOGI(s_tag, "Creating splash screen with logo");
    
    // Splash screen container - tam ekran siyah
    splash_screen = lv_obj_create(lv_scr_act());
    lv_obj_set_size(splash_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(splash_screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(splash_screen, 0, 0);
    lv_obj_set_style_radius(splash_screen, 0, 0);
    lv_obj_set_style_opa(splash_screen, LV_OPA_TRANSP, 0); // Başlangıçta transparan
    lv_obj_clear_flag(splash_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Logo container (merkezde konumlandırmak için)
    lv_obj_t *logo_container = lv_obj_create(splash_screen);
    lv_obj_set_size(logo_container, 800, 480); // ekran boyutunda container
    lv_obj_center(logo_container);
    lv_obj_set_style_bg_opa(logo_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(logo_container, 0, 0);
    lv_obj_clear_flag(logo_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Gerçek logo image - marine_logo.c dosyasından
    logo_img = lv_img_create(logo_container);
    lv_img_set_src(logo_img, &marine_logo); // Kendi logonuzu kullan
    
    // Logo boyutunu ve görünümünü ayarla
    lv_obj_set_style_img_recolor_opa(logo_img, LV_OPA_0, 0); // Renk değişimini kapat
    lv_img_set_zoom(logo_img, 256); // 2x zoom (256 = 1x, 512 = 2x)
    // lv_img_set_zoom(logo_img, 768); // 3x zoom için bu satırı kullan
    
    lv_obj_center(logo_img);
    
    // Company name altında (opsiyonel)
    lv_obj_t *company_label = lv_label_create(logo_container);
    lv_label_set_text(company_label, "WELCOME IOTROCOP TECHNOLOGY");
    lv_obj_set_style_text_color(company_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(company_label, &lv_font_montserrat_14, 0);
    lv_obj_align(company_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    
    // Fade in animasyonu başlat
    lv_anim_init(&fade_anim);
    lv_anim_set_var(&fade_anim, splash_screen);
    lv_anim_set_exec_cb(&fade_anim, splash_fade_anim_cb);
    lv_anim_set_values(&fade_anim, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&fade_anim, 1000); // 1 saniye fade in
    lv_anim_start(&fade_anim);
    
    // Timer başlat
    splash_timer = lv_timer_create(splash_timer_cb, 1200, NULL); // Animasyon bitince çalışır
    splash_active = true;
    splash_phase = 0;
    
    ESP_LOGI(s_tag, "Splash screen created with real logo and fade in started");
}

// Splash screen temizleme
static void destroy_splash_screen(void)
{
    if (splash_screen) {
        lv_obj_del(splash_screen);
        splash_screen = NULL;
        logo_img = NULL;
        ESP_LOGI(s_tag, "Splash screen destroyed");
    }
}

// Navigation button event callback
static void nav_button_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED)
    {
        // Find which navigation button was pressed
        for (int i = 0; i < PAGE_COUNT; i++)
        {
            if (gui.nav_buttons[i] == btn)
            {
                // Sayfa indeksleri 1 artırıldı (MAIN kaldırıldığı için)
                usrGraphicalInterface_showPage((page_type_t)(i + 1));
                ESP_LOGI(s_tag, "Navigation to page %d (%s)", i + 1, page_names[i]);
                break;
            }
        }
    }
}

static void password_defined_callback(bool success)
{
    if (success) {
        ESP_LOGI(s_tag, "Password successfully defined, showing main interface");
        // Şifre başarıyla tanımlandı, şifre sayfasını gizle ve BUTTON sayfasını göster
        usrDefinePassword_hide();
        usrGraphicalInterface_showPage(PAGE_BUTTON_CONTROL); // MAIN yerine BUTTON
    } else {
        ESP_LOGW(s_tag, "Password definition failed or cancelled");
        // Şifre tanımlama başarısız, BUTTON sayfasını göster
        usrDefinePassword_hide();
        usrGraphicalInterface_showPage(PAGE_BUTTON_CONTROL); // MAIN yerine BUTTON
    }
}

void usrGraphicalInterface_init(void)
{
    if (gui.is_initialized)
    {
        ESP_LOGW(s_tag, "GUI already initialized");
        return;
    }

    // Create main container
    gui.main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(gui.main_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(gui.main_container, lv_color_hex(0x0A0A0A), 0); // Koyu gri
    lv_obj_set_style_border_width(gui.main_container, 0, 0);
    lv_obj_set_style_radius(gui.main_container, 0, 0);
    lv_obj_clear_flag(gui.main_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Ana container'ı başlangıçta gizle (splash screen bitince gösterilecek)
    lv_obj_add_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);

    // Create navigation bar
    gui.navigation_bar = lv_obj_create(gui.main_container);
    lv_obj_set_size(gui.navigation_bar, LV_HOR_RES, 80);
    lv_obj_align(gui.navigation_bar, LV_ALIGN_TOP_LEFT, -20, -20);
    lv_obj_set_style_bg_color(gui.navigation_bar, lv_color_hex(0x1E1E1E), 0); // Koyu gri
    lv_obj_set_style_border_width(gui.navigation_bar, 0, 0);
    lv_obj_clear_flag(gui.navigation_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Create navigation buttons
    int btn_width = LV_HOR_RES / PAGE_COUNT;
    
    // Renk paleti
    lv_color_t page_colors[PAGE_COUNT] = {
        lv_color_hex(0x92C1C1), // rgb(146,193,193) - Ana teal rengi
        lv_color_hex(0x5A9999), // Daha koyu teal tonu
        lv_color_hex(0x708080), // Gri-teal karışımı
        lv_color_hex(0x606060)  // Koyu gri
    };
    
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        gui.nav_buttons[i] = lv_btn_create(gui.navigation_bar);
        lv_obj_set_size(gui.nav_buttons[i], btn_width - 4, 50);
        lv_obj_set_pos(gui.nav_buttons[i], i * btn_width + 2 - 20, -5);
        
         lv_obj_set_style_bg_color(gui.nav_buttons[i], page_colors[i], 0);
        lv_obj_set_style_shadow_width(gui.nav_buttons[i], 3, 0); // Daha az gölge
        lv_obj_set_style_shadow_color(gui.nav_buttons[i], lv_color_hex(0x181818), 0); // Koyu gölge
        lv_obj_set_style_radius(gui.nav_buttons[i], 5, 0);

        // Pressed durumu için beyaz yerine açık gri
        lv_obj_set_style_bg_color(gui.nav_buttons[i], lv_color_hex(0xFBFDFD), LV_STATE_PRESSED); // rgb(251,253,253)

        // Create navigation button label
        gui.nav_labels[i] = lv_label_create(gui.nav_buttons[i]);
        lv_label_set_text(gui.nav_labels[i], page_names[i]);
        lv_obj_set_style_text_font(gui.nav_labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(gui.nav_labels[i], lv_color_white(), 0);
        lv_obj_center(gui.nav_labels[i]);

        // Add event callback
        lv_obj_add_event_cb(gui.nav_buttons[i], nav_button_event_cb, LV_EVENT_CLICKED, NULL);
    }

    // Create content area
    gui.content_area = lv_obj_create(gui.main_container);
    lv_obj_set_size(gui.content_area, LV_HOR_RES, LV_VER_RES - 60);
    lv_obj_set_pos(gui.content_area, 0, 60);
    lv_obj_set_style_bg_color(gui.content_area, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_border_width(gui.content_area, 0, 0);
    lv_obj_clear_flag(gui.content_area, LV_OBJ_FLAG_SCROLLABLE);

    // Initialize all pages
    usrButtonPage_init(gui.content_area);
    usrBatteryMonitorPage_init(gui.content_area);
    usrTankLevelPage_init(gui.content_area);
    usrShuntPage_init(gui.content_area);   

    // Initialize password define page
    usrDefinePassword_init(gui.main_container, password_defined_callback);

    // Set initial page - MAIN yerine BUTTON
    gui.current_page = PAGE_BUTTON_CONTROL;
    gui.is_initialized = true;

    // Splash screen'i başlat
    create_splash_screen();

    ESP_LOGI(s_tag, "Graphical interface initialized with splash screen");
}

void usrGraphicalInterface_destroy(void)
{
    if (!gui.is_initialized)
    {
        return;
    }

    // Splash screen temizle
    if (splash_timer) {
        lv_timer_del(splash_timer);
        splash_timer = NULL;
    }
    destroy_splash_screen();

    // Destroy all pages
    usrButtonPage_destroy();
    usrBatteryMonitorPage_destroy();
    usrTankLevelPage_destroy();
    usrShuntPage_destroy();
    usrDefinePassword_destroy();

    // Destroy main container
    if (gui.main_container != NULL)
    {
        lv_obj_del(gui.main_container);
    }

    // Reset GUI structure
    memset(&gui, 0, sizeof(usrGraphicalInterface_t));

    ESP_LOGI(s_tag, "Graphical interface destroyed");
}

void usrGraphicalInterface_showPage(page_type_t page)
{
    if (!gui.is_initialized || page >= PAGE_COUNT + 1) // +1 çünkü MAIN enum'da hala var
    {
        ESP_LOGW(s_tag, "Invalid page or GUI not initialized");
        return;
    }

    // Ana container'ı göster (splash screen bittiyse)
    if (!splash_active && gui.main_container) {
        lv_obj_clear_flag(gui.main_container, LV_OBJ_FLAG_HIDDEN);
    }

    // Hide all pages first
    usrButtonPage_hide();
    usrBatteryMonitorPage_hide();
    usrTankLevelPage_hide();
    usrShuntPage_hide();

    // Update navigation button states - Sadece mevcut butonlar için
    for (int i = 0; i < PAGE_COUNT; i++)
    {
        // Sayfa indeksi 1 artırıldı (MAIN kaldırıldığı için)
        if ((i + 1) == page)
        {
            // Aktif sayfa - beyaz arka plan, siyah yazı
            lv_obj_set_style_bg_color(gui.nav_buttons[i], lv_color_hex(0xFBFDFD), 0); // Açık beyaz
            lv_obj_set_style_text_color(gui.nav_labels[i], lv_color_hex(0x181818), 0); // Koyu yazı
        }
        else
        {
            // Pasif sayfa - orijinal renkler
            lv_color_t page_colors[PAGE_COUNT] = {
                lv_color_hex(0x92C1C1), // Ana teal
                lv_color_hex(0x5A9999), // Koyu teal
                lv_color_hex(0x708080), // Gri-teal
                lv_color_hex(0x606060)  // Koyu gri
            };
            
            lv_obj_set_style_bg_color(gui.nav_buttons[i], page_colors[i], 0);
            lv_obj_set_style_text_color(gui.nav_labels[i], lv_color_hex(0xFBFDFD), 0); // Açık yazı
        }
    }

    switch (page)
    {
    case PAGE_MAIN_MENU:
        // MAIN sayfası kaldırıldı - varsayılan olarak BUTTON göster
        ESP_LOGW(s_tag, "MAIN page removed, showing BUTTON page instead");
        usrButtonPage_show();
        gui.current_page = PAGE_BUTTON_CONTROL;
        break;

    case PAGE_BUTTON_CONTROL:
        usrButtonPage_show();
        break;

    case PAGE_BATTERY_MONITOR:
        usrBatteryMonitorPage_show();
        break;

    case PAGE_TANK_LEVEL:
        usrTankLevelPage_show();
        break;

    case PAGE_SHUNT_MONITOR:
        usrShuntPage_show();
        break;

    default:
        ESP_LOGW(s_tag, "Unknown page type: %d", page);
        return;
    }

    gui.current_page = page;
    
    // Log için sayfa ismini bul
    const char* current_page_name = "UNKNOWN";
    if (page == PAGE_BUTTON_CONTROL) current_page_name = "BUTTONS";
    else if (page == PAGE_BATTERY_MONITOR) current_page_name = "BATTERY";
    else if (page == PAGE_TANK_LEVEL) current_page_name = "TANK LEVEL";
    else if (page == PAGE_SHUNT_MONITOR) current_page_name = "SHUNT";
    
    ESP_LOGI(s_tag, "Switched to page: %s", current_page_name);
}

bool usrGraphicalInterface_isInitialized(void)
{
    return gui.is_initialized;
}

void buttonMenu(void)
{
    ESP_LOGI(s_tag, "Legacy buttonMenu() called - initializing new GUI system");
    usrGraphicalInterface_init();
}

// Hide navigation bar
void usrGraphicalInterface_hideNavbar(void)
{
    if (gui.navigation_bar) {
        lv_obj_add_flag(gui.navigation_bar, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(s_tag, "Navigation bar hidden");
        
        // Content area'yı navbar olmadan tam ekran yap
        if (gui.content_area) {
            lv_obj_set_size(gui.content_area, LV_HOR_RES, LV_VER_RES);
            lv_obj_set_pos(gui.content_area, 0, 0);
        }
    }
}

// Show navigation bar
void usrGraphicalInterface_showNavbar(void)
{
    if (gui.navigation_bar) {
        lv_obj_clear_flag(gui.navigation_bar, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(s_tag, "Navigation bar shown");
        
        // Content area'yı navbar altına taşı
        if (gui.content_area) {
            lv_obj_set_size(gui.content_area, LV_HOR_RES, LV_VER_RES - 60);
            lv_obj_set_pos(gui.content_area, 0, 60);
        }
    }
}

// Splash screen manuel olarak atlamak için (test amaçlı)
void usrGraphicalInterface_skipSplash(void)
{
    if (splash_active && splash_timer) {
        splash_phase = 2; // Direkt fade out'a geç
        splash_timer_cb(splash_timer);
        ESP_LOGI(s_tag, "Splash screen skipped");
    }
}