// usrTankSettingsPage.c - Düzeltilmiş versiyon

#include "usrTankSettingsPage.h"
#include "usrGraphicalInterface.h"

// DÜZELTME 1: Farklı namespace'ler kullan
#define NVS_NAMESPACE_NAMES "tank_names"        // Tank names için
#define NVS_KEY_TANK_NAMES "names"


static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Primary corporate color
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Light background
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Dark text/borders


// Font reference
extern lv_font_t lv_font_montserrat_10_omega;

static const char *s_tag = "usrTankSettingsPage";
static usrTankSettingsPage_t settingsPage = {0};

// Default tank names
static const char default_tank_names[8][16] = {
    "0-190 Ohm T1", "0-190 Ohm T2", "0-190 Ohm T3", "0-190 Ohm T4",
    "30-240 Ohm T1", "30-240 Ohm T2", "30-240 Ohm T3", "30-240 Ohm T4"
};

// Forward declarations
static void back_button_cb(lv_event_t *e);
static void save_button_cb(lv_event_t *e);
static void reset_button_cb(lv_event_t *e);
static void textarea_focus_cb(lv_event_t *e);
static void keyboard_ready_cb(lv_event_t *e);
static void page_click_cb(lv_event_t *e);
static esp_err_t save_tank_names_to_nvs(char tank_names[8][16]);
static esp_err_t load_tank_names_from_nvs(char tank_names[8][16]);
static void create_unified_tank_widget(int tank_index, int row, int col);

// Save tank names to NVS - DÜZELTME 2: Doğru namespace kullan
static esp_err_t save_tank_names_to_nvs(char tank_names[8][16])
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_NAMES, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS names: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(nvs_handle, NVS_KEY_TANK_NAMES, tank_names, 8 * 16);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        ESP_LOGI(s_tag, "Tank names saved to NVS successfully");
    } else {
        ESP_LOGE(s_tag, "Failed to save tank names: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    return ret;
}

// Load tank names from NVS - DÜZELTME 3: Doğru namespace kullan
static esp_err_t load_tank_names_from_nvs(char tank_names[8][16])
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_NAMES, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(s_tag, "Error opening NVS names for reading: %s", esp_err_to_name(ret));
        // Use defaults
        for (int i = 0; i < 8; i++) {
            strncpy(tank_names[i], default_tank_names[i], 16);
        }
        return ret;
    }

    size_t required_size = 8 * 16;
    ret = nvs_get_blob(nvs_handle, NVS_KEY_TANK_NAMES, tank_names, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(s_tag, "No saved tank names found - using defaults");
        for (int i = 0; i < 8; i++) {
            strncpy(tank_names[i], default_tank_names[i], 16);
        }
    } else if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error reading tank names: %s", esp_err_to_name(ret));
        for (int i = 0; i < 8; i++) {
            strncpy(tank_names[i], default_tank_names[i], 16);
        }
    } else {
        ESP_LOGI(s_tag, "Tank names loaded from NVS successfully");
    }

    nvs_close(nvs_handle);
    return ret;
}



// Back button callback
static void back_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Back button pressed");
    
    if (settingsPage.back_callback) {
        settingsPage.back_callback();
    }
}

// Save button callback - DÜZELTME 6: Kaydetmeden önce checkbox durumlarını sync et
static void save_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Save button pressed");
    
    char tank_names[8][16];
    bool save_names_success = false;
    
    
    // Get all textarea values
    for (int i = 0; i < 8; i++) {
        const char *text = lv_textarea_get_text(settingsPage.tank_name_textareas[i]);
        if (strlen(text) > 0) {
            strncpy(tank_names[i], text, 15);
            tank_names[i][15] = '\0';
        } else {
            strncpy(tank_names[i], default_tank_names[i], 16);
        }
    }
    
    // Tank names kaydet
    esp_err_t ret_names = save_tank_names_to_nvs(tank_names);
    save_names_success = (ret_names == ESP_OK);
    
    
    
    // Update status based on save results
    if (save_names_success) {
        lv_label_set_text(settingsPage.status_label, "Tank names saved successfully!");
        lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        settingsPage.is_modified = false;
    }else {
        lv_label_set_text(settingsPage.status_label, "Failed to save tank names!");
        lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

// Reset button callback
static void reset_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Reset button pressed");
    
    // Reset all textareas to default values
    for (int i = 0; i < 8; i++) {
        lv_textarea_set_text(settingsPage.tank_name_textareas[i], default_tank_names[i]);
    }
    
    lv_label_set_text(settingsPage.status_label, "Tank names reset to defaults");
    lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    settingsPage.is_modified = true;
}

// Textarea focus callback
static void textarea_focus_cb(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);

    if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        lv_obj_scroll_to_view(ta, LV_ANIM_ON);
        lv_keyboard_set_textarea(settingsPage.keyboard, ta);
        lv_obj_clear_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(settingsPage.keyboard, LV_ALIGN_BOTTOM_MID, -18, 0);
    } else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(settingsPage.keyboard, NULL);
        lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    settingsPage.is_modified = true;
}

// Keyboard ready callback
static void keyboard_ready_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Keyboard ready callback - hiding keyboard");

    if (settingsPage.keyboard) {
        lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_scroll_to_y(settingsPage.page_container, 0, LV_ANIM_ON);
    }

    lv_label_set_text(settingsPage.status_label, "Press Save to store changes");
    lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
}

// Page click callback
static void page_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    
    if (settingsPage.keyboard && !lv_obj_has_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN)) {
        bool is_textarea = false;
        bool is_keyboard_area = false;
        
        // Textarea check
        for (int i = 0; i < 8; i++) {
            if (target == settingsPage.tank_name_textareas[i] || 
                target == settingsPage.tank_name_containers[i]) {
                is_textarea = true;
                break;
            }
        }
        
        // Keyboard area check
        if (target == settingsPage.keyboard) {
            is_keyboard_area = true;
        }
        
        // Hide keyboard if clicked outside
        if (!is_textarea && !is_keyboard_area) {
            lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_obj_scroll_to_y(settingsPage.page_container, 0, LV_ANIM_ON);
            ESP_LOGI(s_tag, "Keyboard hidden due to outside click");
        }
    }
}

// Create unified tank widget
static void create_unified_tank_widget(int tank_index, int row, int col)
{
    int container_width = 165;
    int container_height = 100;
    int spacing_x = 15;
    int spacing_y = 20;
    int start_x = 10;
    int start_y = 80;
    
    int x = start_x + col * (container_width + spacing_x);
    int y = start_y + row * (container_height + spacing_y);
    
    // Create main container
    settingsPage.tank_name_containers[tank_index] = lv_obj_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.tank_name_containers[tank_index], container_width, container_height);
    lv_obj_set_pos(settingsPage.tank_name_containers[tank_index], x, y);
    lv_obj_set_style_bg_color(settingsPage.tank_name_containers[tank_index], lv_color_make(20, 20, 20), 0);
    lv_obj_set_style_border_color(settingsPage.tank_name_containers[tank_index], lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(settingsPage.tank_name_containers[tank_index], 1, 0);
    lv_obj_set_style_radius(settingsPage.tank_name_containers[tank_index], 5, 0);
    lv_obj_set_style_pad_all(settingsPage.tank_name_containers[tank_index], 8, 0);
    
    // Create tank name label
    settingsPage.tank_name_labels[tank_index] = lv_label_create(settingsPage.tank_name_containers[tank_index]);
    lv_label_set_text_fmt(settingsPage.tank_name_labels[tank_index], "Tank %d Name:", tank_index + 1);
    lv_obj_set_style_text_font(settingsPage.tank_name_labels[tank_index], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settingsPage.tank_name_labels[tank_index], color_primary, 0);
    lv_obj_set_pos(settingsPage.tank_name_labels[tank_index], 5, 5);
    
    // Create textarea for tank name
    settingsPage.tank_name_textareas[tank_index] = lv_textarea_create(settingsPage.tank_name_containers[tank_index]);
    lv_obj_set_size(settingsPage.tank_name_textareas[tank_index], container_width - 21, 35);
    lv_obj_set_pos(settingsPage.tank_name_textareas[tank_index], 0, 30);
    lv_textarea_set_one_line(settingsPage.tank_name_textareas[tank_index], true);
    lv_textarea_set_max_length(settingsPage.tank_name_textareas[tank_index], 15);
    lv_textarea_set_placeholder_text(settingsPage.tank_name_textareas[tank_index], "Tank name");
    
    // Style textarea
    lv_obj_set_style_bg_color(settingsPage.tank_name_textareas[tank_index], lv_color_make(40, 40, 40), 0);
    lv_obj_set_style_text_color(settingsPage.tank_name_textareas[tank_index], lv_color_white(), 0);
    lv_obj_set_style_border_color(settingsPage.tank_name_textareas[tank_index], lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_border_width(settingsPage.tank_name_textareas[tank_index], 1, 0);
    lv_obj_set_style_radius(settingsPage.tank_name_textareas[tank_index], 3, 0);
    
    // Add event callback
    lv_obj_add_event_cb(settingsPage.tank_name_textareas[tank_index], textarea_focus_cb, LV_EVENT_FOCUSED, NULL);

    
    
    
}



// Initialize settings page - DÜZELTME 10: Başlatma sırasında NVS'den yükle
void usrTankSettingsPage_init(lv_obj_t *parent)
{
    if (settingsPage.page_container != NULL) {
        ESP_LOGW(s_tag, "Settings page already initialized");
        return;
    }
    
    
    // Create main container
    settingsPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(settingsPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(settingsPage.page_container, 0, 0);
    lv_obj_set_style_bg_color(settingsPage.page_container, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_border_width(settingsPage.page_container, 0, 0);
    lv_obj_set_style_pad_all(settingsPage.page_container, 0, 0);
    
    // Configure scrolling
    lv_obj_set_scroll_dir(settingsPage.page_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(settingsPage.page_container, LV_SCROLLBAR_MODE_ON);
    lv_obj_add_flag(settingsPage.page_container, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_snap_y(settingsPage.page_container, LV_SCROLL_SNAP_NONE);
    
    // Style scrollbar
    lv_obj_set_style_bg_color(settingsPage.page_container, lv_color_white(), LV_PART_SCROLLBAR);
    lv_obj_set_style_width(settingsPage.page_container, 12, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(settingsPage.page_container, LV_OPA_70, LV_PART_SCROLLBAR);
    
    // Page click event
    lv_obj_add_event_cb(settingsPage.page_container, page_click_cb, LV_EVENT_CLICKED, NULL);
    
    // Create back button
    settingsPage.back_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.back_button, 80, 40);
    lv_obj_set_pos(settingsPage.back_button, 10, 5);
    lv_obj_set_style_bg_color(settingsPage.back_button, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(settingsPage.back_button, 5, 0);
    lv_obj_add_event_cb(settingsPage.back_button, back_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(settingsPage.back_button, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_label = lv_label_create(settingsPage.back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    
    // Create title
    settingsPage.title_label = lv_label_create(settingsPage.page_container);
    lv_label_set_text(settingsPage.title_label, "TANK SETTINGS");
    lv_obj_set_style_text_font(settingsPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(settingsPage.title_label, color_primary, 0);
    lv_obj_set_pos(settingsPage.title_label, 290, 15);
    
    
    // Create unified tank widgets (4x2 grid)
    for (int i = 0; i < 8; i++) {
        int row = i / 4;
        int col = i % 4;
        create_unified_tank_widget(i, row, col);
    }
    
    // Create Save button
    settingsPage.save_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.save_button, 120, 45);
    lv_obj_set_pos(settingsPage.save_button, 235, 330);
    lv_obj_set_style_bg_color(settingsPage.save_button, color_primary, 0);
    lv_obj_set_style_radius(settingsPage.save_button, 5, 0);
    lv_obj_add_event_cb(settingsPage.save_button, save_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(settingsPage.save_button, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *save_label = lv_label_create(settingsPage.save_button);
    lv_label_set_text(save_label, LV_SYMBOL_SAVE " SAVE");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(save_label, color_dark, 0);
    lv_obj_center(save_label);
    
    // Create Reset button
    settingsPage.reset_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.reset_button, 120, 45);
    lv_obj_set_pos(settingsPage.reset_button, 370, 330);
    lv_obj_set_style_bg_color(settingsPage.reset_button, color_dark, 0);
    lv_obj_set_style_radius(settingsPage.reset_button, 5, 0);
    lv_obj_add_event_cb(settingsPage.reset_button, reset_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(settingsPage.reset_button, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *reset_label = lv_label_create(settingsPage.reset_button);
    lv_label_set_text(reset_label, LV_SYMBOL_REFRESH " RESET");
    lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(reset_label, color_secondary, 0);
    lv_obj_center(reset_label);
    
    // Create status label
    settingsPage.status_label = lv_label_create(settingsPage.page_container);
    lv_label_set_text(settingsPage.status_label, "Modify tank names then press Save All");
    lv_obj_set_style_text_font(settingsPage.status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(settingsPage.status_label, LV_ALIGN_TOP_MID, -35, 420);

    // Create keyboard (initially positioned, will be repositioned dynamically)
    settingsPage.keyboard = lv_keyboard_create(parent);
    lv_keyboard_set_mode(settingsPage.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_size(settingsPage.keyboard, LV_HOR_RES - 95, 135);
    lv_obj_set_pos(settingsPage.keyboard, 60, 200);
    lv_obj_move_foreground(settingsPage.keyboard);
    lv_obj_add_event_cb(settingsPage.keyboard, keyboard_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(settingsPage.keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(settingsPage.keyboard, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_border_width(settingsPage.keyboard, 2, 0);
    lv_obj_set_style_border_color(settingsPage.keyboard, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_radius(settingsPage.keyboard, 5, 0); 
    
    // Set content height for scrolling
    lv_obj_set_content_height(settingsPage.page_container, 500);
    
    // Initialize state
    settingsPage.is_active = false;
    settingsPage.is_modified = false;
    settingsPage.current_textarea = NULL;
    settingsPage.back_callback = NULL;
    
    // Load tank names and set them
    char tank_names[8][16];
    load_tank_names_from_nvs(tank_names);
    for (int i = 0; i < 8; i++) {
        lv_textarea_set_text(settingsPage.tank_name_textareas[i], tank_names[i]);
    }
    
    // Hide page initially
    usrTankSettingsPage_hide();
    
    ESP_LOGI(s_tag, "Tank settings page initialized with NVS data loaded");
}

// Show settings page 
void usrTankSettingsPage_show(void)
{
    if (settingsPage.page_container != NULL) {
        usrGraphicalInterface_hideNavbar(); 
        lv_obj_clear_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_scroll_to_y(settingsPage.page_container, 0, LV_ANIM_OFF);
        settingsPage.is_active = true;
        
        
        ESP_LOGI(s_tag, "Settings page shown");
    }
    
    // Show all UI elements
    if (settingsPage.back_button) {
        lv_obj_clear_flag(settingsPage.back_button, LV_OBJ_FLAG_HIDDEN);
    }
    if (settingsPage.title_label) {
        lv_obj_clear_flag(settingsPage.title_label, LV_OBJ_FLAG_HIDDEN);
    }
}

// Hide settings page
void usrTankSettingsPage_hide(void)
{
    if (settingsPage.page_container != NULL) {
        lv_obj_add_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
        settingsPage.is_active = false;
        
        // Hide keyboard if visible
        if (settingsPage.keyboard) {
            lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Hide UI elements
        if (settingsPage.back_button) {
            lv_obj_add_flag(settingsPage.back_button, LV_OBJ_FLAG_HIDDEN);
        }
        if (settingsPage.title_label) {
            lv_obj_add_flag(settingsPage.title_label, LV_OBJ_FLAG_HIDDEN);
        }
        
        usrGraphicalInterface_showNavbar(); 
        
        ESP_LOGI(s_tag, "Settings page hidden and navbar shown");
    }
}

// Destroy settings page
void usrTankSettingsPage_destroy(void)
{
    if (settingsPage.page_container != NULL) {
        usrGraphicalInterface_showNavbar();
        lv_obj_del(settingsPage.page_container);
        
        // Clean up individual elements
        if (settingsPage.back_button) {
            lv_obj_del(settingsPage.back_button);
        }
        if (settingsPage.title_label) {
            lv_obj_del(settingsPage.title_label);
        }
        if (settingsPage.keyboard) {
            lv_obj_del(settingsPage.keyboard);
        }
        
        memset(&settingsPage, 0, sizeof(usrTankSettingsPage_t));
        ESP_LOGI(s_tag, "Settings page destroyed and navbar restored");
    }
}

// Check if settings page is active
bool usrTankSettingsPage_isActive(void)
{
    return settingsPage.is_active;
}

// Set back callback
void usrTankSettingsPage_setBackCallback(void (*callback)(void))
{
    settingsPage.back_callback = callback;
}

// Load tank names (for external use)
void usrTankSettingsPage_loadTankNames(char tank_names[8][16])
{
    load_tank_names_from_nvs(tank_names);
}

// Save tank names (for external use)
void usrTankSettingsPage_saveTankNames(void)
{
    save_button_cb(NULL);
}
