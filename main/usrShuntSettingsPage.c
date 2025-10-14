#include "usrShuntSettingsPage.h"
#include "usrGraphicalInterface.h"

// NVS namespaces - farklı namespace'ler kullan
#define NVS_NAMESPACE_SHUNT_NAMES "shunt_names"        // Shunt names için
#define NVS_KEY_SHUNT_NAMES "names"


static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Primary corporate color
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Light background
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Dark text/borders
static const lv_color_t color_main = LV_COLOR_MAKE(146, 193, 193);

static const char *s_tag = "usrShuntSettingsPage";
static usrShuntSettingsPage_t settingsPage = {0};

// Default shunt names (Main Shunt + 4 Quadro Shunts)
static const char default_shunt_names[5][16] = {
    "MAIN SHUNT",
    "SHUNT 1",
    "SHUNT 2", 
    "SHUNT 3",
    "SHUNT 4"
};

// Forward declarations
static void back_button_cb(lv_event_t *e);
static void save_button_cb(lv_event_t *e);
static void reset_button_cb(lv_event_t *e);
static void textarea_focus_cb(lv_event_t *e);
static void keyboard_ready_cb(lv_event_t *e);
static void page_click_cb(lv_event_t *e);
static esp_err_t save_shunt_names_to_nvs(char shunt_names[5][16]);
static esp_err_t load_shunt_names_from_nvs(char shunt_names[5][16]);
static void create_unified_shunt_widget(int shunt_index, int row, int col);


// Save shunt names to NVS
static esp_err_t save_shunt_names_to_nvs(char shunt_names[5][16])
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SHUNT_NAMES, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS names: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(nvs_handle, NVS_KEY_SHUNT_NAMES, shunt_names, 5 * 16);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        ESP_LOGI(s_tag, "Shunt names saved to NVS successfully");
    } else {
        ESP_LOGE(s_tag, "Failed to save shunt names: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    return ret;
}

// Load shunt names from NVS
static esp_err_t load_shunt_names_from_nvs(char shunt_names[5][16])
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_SHUNT_NAMES, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(s_tag, "Error opening NVS names for reading: %s", esp_err_to_name(ret));
        // Use defaults
        for (int i = 0; i < 5; i++) {
            strncpy(shunt_names[i], default_shunt_names[i], 16);
        }
        return ret;
    }

    size_t required_size = 5 * 16;
    ret = nvs_get_blob(nvs_handle, NVS_KEY_SHUNT_NAMES, shunt_names, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(s_tag, "No saved shunt names found - using defaults");
        for (int i = 0; i < 5; i++) {
            strncpy(shunt_names[i], default_shunt_names[i], 16);
        }
    } else if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error reading shunt names: %s", esp_err_to_name(ret));
        for (int i = 0; i < 5; i++) {
            strncpy(shunt_names[i], default_shunt_names[i], 16);
        }
    } else {
        ESP_LOGI(s_tag, "Shunt names loaded from NVS successfully");
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

// Save button callback
static void save_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Save button pressed");
    
    char shunt_names[5][16];
    bool save_names_success = false;
    
    
    // Get all textarea values
    for (int i = 0; i < 5; i++) {
        const char *text = lv_textarea_get_text(settingsPage.shunt_name_textareas[i]);
        if (strlen(text) > 0) {
            strncpy(shunt_names[i], text, 15);
            shunt_names[i][15] = '\0';
        } else {
            strncpy(shunt_names[i], default_shunt_names[i], 16);
        }
    }
    
    // Save shunt names
    esp_err_t ret_names = save_shunt_names_to_nvs(shunt_names);
    save_names_success = (ret_names == ESP_OK);
    
    
    // Update status based on save results
    if (save_names_success) {
        lv_label_set_text(settingsPage.status_label, "Shunt names saved successfully!");
        lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        settingsPage.is_modified = false;
    }else {
        lv_label_set_text(settingsPage.status_label, "Failed to save shunt names!");
        lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

// Reset button callback
static void reset_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Reset button pressed");
    
    // Reset all textareas to default values
    for (int i = 0; i < 5; i++) {
        lv_textarea_set_text(settingsPage.shunt_name_textareas[i], default_shunt_names[i]);
    }
    
    lv_label_set_text(settingsPage.status_label, "Shunt names reset to defaults");
    lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
    settingsPage.is_modified = true;
}

// Textarea focus callback
static void textarea_focus_cb(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);

    if (lv_event_get_code(e) == LV_EVENT_FOCUSED) {
        ESP_LOGI(s_tag, "Textarea focused");
        
        // Klavyeyi göster ve textarea'ya bağla
        lv_keyboard_set_textarea(settingsPage.keyboard, ta);
        lv_obj_clear_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(settingsPage.keyboard);
        
        // Klavye boyutunu ve pozisyonunu ayarla
        lv_obj_set_size(settingsPage.keyboard, LV_HOR_RES - 85, 135);
        lv_obj_align(settingsPage.keyboard, LV_ALIGN_BOTTOM_MID, -18, -10);
        
    } else if (lv_event_get_code(e) == LV_EVENT_DEFOCUSED) {
        ESP_LOGI(s_tag, "Textarea defocused");
        
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
        for (int i = 0; i < 5; i++) {
            if (target == settingsPage.shunt_name_textareas[i]) {
                is_textarea = true;
                break;
            }
        }
        
        // Keyboard area check
        if (target == settingsPage.keyboard) {
            is_keyboard_area = true;
        }
        
        // Hide keyboard if clicked outside textarea and keyboard
        if (!is_textarea && !is_keyboard_area) {
            lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
            ESP_LOGI(s_tag, "Keyboard hidden due to outside click");
        }
    }
}

// Create unified shunt widget
static void create_unified_shunt_widget(int shunt_index, int row, int col)
{
    int container_width = 225;
    int container_height = 100;
    int spacing_x = 20;
    int spacing_y = 20;
    int start_x = 5;
    int start_y = 70;
    
    // 3 column layout için x pozisyonunu hesapla
    int x = start_x + col * (container_width + spacing_x);
    int y = start_y + row * (container_height + spacing_y);
    
    // Create main container
    settingsPage.shunt_name_containers[shunt_index] = lv_obj_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.shunt_name_containers[shunt_index], container_width, container_height);
    lv_obj_set_pos(settingsPage.shunt_name_containers[shunt_index], x, y);
    
    // Container styling - shunt tipine göre renk
    if (shunt_index == 0) {
        // Main Shunt - Red border
        lv_obj_set_style_bg_color(settingsPage.shunt_name_containers[shunt_index], lv_color_make(30, 20, 20), 0);
        lv_obj_set_style_border_color(settingsPage.shunt_name_containers[shunt_index], color_main, 0);
    } else {
        // Quadro Shunts - Blue border
        lv_obj_set_style_bg_color(settingsPage.shunt_name_containers[shunt_index], lv_color_make(20, 20, 30), 0);
        lv_obj_set_style_border_color(settingsPage.shunt_name_containers[shunt_index], lv_palette_main(LV_PALETTE_BLUE), 0);
    }
    
    lv_obj_set_style_border_width(settingsPage.shunt_name_containers[shunt_index], 2, 0);
    lv_obj_set_style_radius(settingsPage.shunt_name_containers[shunt_index], 8, 0);
    lv_obj_set_style_pad_all(settingsPage.shunt_name_containers[shunt_index], 10, 0);
    
    
    // Create textarea for shunt name
    settingsPage.shunt_name_textareas[shunt_index] = lv_textarea_create(settingsPage.shunt_name_containers[shunt_index]);
    lv_obj_set_size(settingsPage.shunt_name_textareas[shunt_index], container_width - 31, 5);
    lv_obj_set_pos(settingsPage.shunt_name_textareas[shunt_index], 3, 30);
    lv_textarea_set_one_line(settingsPage.shunt_name_textareas[shunt_index], true);
    lv_textarea_set_max_length(settingsPage.shunt_name_textareas[shunt_index], 15);
    lv_textarea_set_placeholder_text(settingsPage.shunt_name_textareas[shunt_index], "Shunt name");

    // Create shunt type label
    settingsPage.shunt_name_labels[shunt_index] = lv_label_create(settingsPage.shunt_name_containers[shunt_index]);
    if (shunt_index == 0) {
        lv_label_set_text_fmt(settingsPage.shunt_name_labels[shunt_index], "Main Shunt Name (300A):");
        lv_obj_set_style_text_color(settingsPage.shunt_name_labels[shunt_index], color_main, 0);        
        lv_obj_set_style_border_color(settingsPage.shunt_name_textareas[shunt_index], color_main, 0);
    } else {
        lv_label_set_text_fmt(settingsPage.shunt_name_labels[shunt_index], "Shunt %d Name (30A):", shunt_index);
        lv_obj_set_style_text_color(settingsPage.shunt_name_labels[shunt_index], color_primary, 0);        
        lv_obj_set_style_border_color(settingsPage.shunt_name_textareas[shunt_index], lv_palette_main(LV_PALETTE_CYAN), 0);
    }
    lv_obj_set_style_text_font(settingsPage.shunt_name_labels[shunt_index], &lv_font_montserrat_12, 0);
    lv_obj_set_pos(settingsPage.shunt_name_labels[shunt_index], 8, 5);
    
    // Style textarea
    lv_obj_set_style_bg_color(settingsPage.shunt_name_textareas[shunt_index], lv_color_make(40, 40, 40), 0);
    lv_obj_set_style_text_color(settingsPage.shunt_name_textareas[shunt_index], lv_color_white(), 0);
    lv_obj_set_style_border_width(settingsPage.shunt_name_textareas[shunt_index], 1, 0);
    lv_obj_set_style_radius(settingsPage.shunt_name_textareas[shunt_index], 5, 0);
    
    // Add event callback
    lv_obj_add_event_cb(settingsPage.shunt_name_textareas[shunt_index], textarea_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(settingsPage.shunt_name_textareas[shunt_index], textarea_focus_cb, LV_EVENT_DEFOCUSED, NULL);

    
}



// Initialize settings page
void usrShuntSettingsPage_init(lv_obj_t *parent)
{
    if (settingsPage.page_container != NULL) {
        ESP_LOGW(s_tag, "Shunt settings page already initialized");
        return;
    }
    
    
    // Create main container
    settingsPage.page_container = lv_obj_create(parent);
    lv_obj_set_size(settingsPage.page_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(settingsPage.page_container, 0, 0);
    lv_obj_set_style_bg_color(settingsPage.page_container, lv_color_make(10, 10, 10), 0);
    lv_obj_set_style_border_width(settingsPage.page_container, 0, 0);
    lv_obj_set_style_pad_all(settingsPage.page_container, 0, 0);
    
    // Page click event
    lv_obj_add_event_cb(settingsPage.page_container, page_click_cb, LV_EVENT_CLICKED, NULL);
    
    // Create back button
    settingsPage.back_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.back_button, 80, 40);
    lv_obj_set_pos(settingsPage.back_button, 10, 0);
    lv_obj_set_style_bg_color(settingsPage.back_button, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_radius(settingsPage.back_button, 5, 0);
    lv_obj_add_event_cb(settingsPage.back_button, back_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *back_label = lv_label_create(settingsPage.back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);
    
    // Create title
    settingsPage.title_label = lv_label_create(settingsPage.page_container);
    lv_label_set_text(settingsPage.title_label, "SHUNT SETTINGS");
    lv_obj_set_style_text_font(settingsPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(settingsPage.title_label, color_primary, 0);
    lv_obj_set_pos(settingsPage.title_label, 260, 15);
    
    // Widget'ları oluştur
    create_unified_shunt_widget(1, 0, 0);  // Quadro 1 - Row 0, Col 0
    create_unified_shunt_widget(0, 0, 1);  // Main Shunt - Row 0, Col 1 (center)
    create_unified_shunt_widget(2, 0, 2);  // Quadro 2 - Row 0, Col 2
    create_unified_shunt_widget(3, 1, 0);  // Quadro 3 - Row 1, Col 0
    create_unified_shunt_widget(4, 1, 2);  // Quadro 4 - Row 1, Col 2
    
    // Save button
    settingsPage.save_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.save_button, 120, 45);
    lv_obj_set_pos(settingsPage.save_button, 237, 245);
    lv_obj_set_style_bg_color(settingsPage.save_button, color_primary, 0);
    lv_obj_set_style_radius(settingsPage.save_button, 5, 0);
    lv_obj_add_event_cb(settingsPage.save_button, save_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *save_label = lv_label_create(settingsPage.save_button);
    lv_label_set_text(save_label, LV_SYMBOL_SAVE " SAVE");
    lv_obj_set_style_text_font(save_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(save_label, color_dark, 0);
    lv_obj_center(save_label);
    
    // Reset button
    settingsPage.reset_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.reset_button, 120, 45);
    lv_obj_set_pos(settingsPage.reset_button, 367, 245);
    lv_obj_set_style_bg_color(settingsPage.reset_button, color_dark, 0);
    lv_obj_set_style_radius(settingsPage.reset_button, 5, 0);
    lv_obj_add_event_cb(settingsPage.reset_button, reset_button_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *reset_label = lv_label_create(settingsPage.reset_button);
    lv_label_set_text(reset_label, LV_SYMBOL_REFRESH " RESET");
    lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(reset_label, color_secondary, 0);
    lv_obj_center(reset_label);
    
    // Status label
    settingsPage.status_label = lv_label_create(settingsPage.page_container);
    lv_label_set_text(settingsPage.status_label, "Modify shunt names, then press Save All");
    lv_obj_set_style_text_font(settingsPage.status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(settingsPage.status_label, LV_ALIGN_TOP_MID, -35, 420);
    
    // Klavyeyi parent'a ekle (sayfa scroll edilirken sabit kalsın)
    settingsPage.keyboard = lv_keyboard_create(parent);
    lv_keyboard_set_mode(settingsPage.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_size(settingsPage.keyboard, LV_HOR_RES - 85, 135);
    lv_obj_align(settingsPage.keyboard, LV_ALIGN_BOTTOM_MID, -18, -5);
    lv_obj_move_foreground(settingsPage.keyboard);
    lv_obj_add_event_cb(settingsPage.keyboard, keyboard_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_flag(settingsPage.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(settingsPage.keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(settingsPage.keyboard, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_border_width(settingsPage.keyboard, 2, 0);
    lv_obj_set_style_border_color(settingsPage.keyboard, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_radius(settingsPage.keyboard, 5, 0);
    
    // Initialize state
    settingsPage.is_active = false;
    settingsPage.is_modified = false;
    settingsPage.current_textarea = NULL;
    settingsPage.back_callback = NULL;
    
    // Load shunt names and set them
    char shunt_names[5][16];
    load_shunt_names_from_nvs(shunt_names);
    for (int i = 0; i < 5; i++) {
        lv_textarea_set_text(settingsPage.shunt_name_textareas[i], shunt_names[i]);
    }
    
    // Hide page initially
    usrShuntSettingsPage_hide();
    
    ESP_LOGI(s_tag, "Shunt settings page initialized");
}

// Show settings page
void usrShuntSettingsPage_show(void)
{
    if (settingsPage.page_container != NULL) {
        usrGraphicalInterface_hideNavbar(); 
        lv_obj_clear_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
        settingsPage.is_active = true;
        
        
        ESP_LOGI(s_tag, "Shunt settings page shown");
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
void usrShuntSettingsPage_hide(void)
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
        
        ESP_LOGI(s_tag, "Shunt settings page hidden and navbar shown");
    }
}

// Destroy settings page
void usrShuntSettingsPage_destroy(void)
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
        
        memset(&settingsPage, 0, sizeof(usrShuntSettingsPage_t));
        ESP_LOGI(s_tag, "Shunt settings page destroyed and navbar restored");
    }
}

// Check if settings page is active
bool usrShuntSettingsPage_isActive(void)
{
    return settingsPage.is_active;
}

// Set back callback
void usrShuntSettingsPage_setBackCallback(void (*callback)(void))
{
    settingsPage.back_callback = callback;
}

// Load shunt names (for external use)
void usrShuntSettingsPage_loadShuntNames(char shunt_names[5][16])
{
    load_shunt_names_from_nvs(shunt_names);
}

// Save shunt names (for external use)
void usrShuntSettingsPage_saveShuntNames(void)
{
    save_button_cb(NULL);
}
