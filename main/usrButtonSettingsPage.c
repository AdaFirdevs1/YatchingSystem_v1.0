// usrButtonSettingsPage.c - Button Settings Page Implementation

#include "usrButtonSettingsPage.h"
#include "usrGraphicalInterface.h"

// NVS namespaces
#define NVS_NAMESPACE_BUTTON_NAMES "button_names"
#define NVS_KEY_BUTTON_NAMES "names"

static const lv_color_t color_primary = LV_COLOR_MAKE(146, 193, 193);     // Primary corporate color
static const lv_color_t color_secondary = LV_COLOR_MAKE(251, 253, 253);   // Light background
static const lv_color_t color_dark = LV_COLOR_MAKE(24, 24, 24);           // Dark text/borders



static const char *s_tag = "usrButtonSettingsPage";
static usrButtonSettingsPage_t settingsPage = {0};

// Default button names
static const char default_button_names[6][11] = {
    "PWM-1", "PWM-2", "PWM-3", "PWM-4", "PWM-5", "PWM-6"
};

// Forward declarations
static void back_button_cb(lv_event_t *e);
static void save_button_cb(lv_event_t *e);
static void reset_button_cb(lv_event_t *e);
static void textarea_focus_cb(lv_event_t *e);
static void keyboard_ready_cb(lv_event_t *e);
static void page_click_cb(lv_event_t *e);
static void create_unified_button_widget(int button_index, int row, int col);


// NVS başlatma fonksiyonu 
esp_err_t usrButtonSettingsPage_initNVS(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(s_tag, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(s_tag, "NVS initialized successfully");
    } else {
        ESP_LOGE(s_tag, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

// Save button names to NVS
esp_err_t save_button_names_to_nvs(char button_names[6][11])
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // NVS açma işlemi
    ret = nvs_open(NVS_NAMESPACE_BUTTON_NAMES, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error opening NVS names: %s", esp_err_to_name(ret));
        return ret;
    }

    // Veri yazma işlemi
    ret = nvs_set_blob(nvs_handle, NVS_KEY_BUTTON_NAMES, button_names, 6 * 11);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Failed to write button names: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    // Commit işlemi
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Failed to commit button names: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    nvs_close(nvs_handle);
    
    ESP_LOGI(s_tag, "Button names saved to NVS successfully");
    
    // Debug: Kaydedilen verileri logla
    for (int i = 0; i < 6; i++) {
        ESP_LOGI(s_tag, "Saved button %d: '%s'", i + 1, button_names[i]);
    }
    
    return ESP_OK;
}

// Load button names from NVS
esp_err_t load_button_names_from_nvs(char button_names[6][11])
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_BUTTON_NAMES, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(s_tag, "Error opening NVS names for reading: %s", esp_err_to_name(ret));
        // Use defaults
        for (int i = 0; i < 6; i++) {
            strncpy(button_names[i], default_button_names[i], 11);
        }
        return ret;
    }

    size_t required_size = 6 * 11;
    ret = nvs_get_blob(nvs_handle, NVS_KEY_BUTTON_NAMES, button_names, &required_size);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(s_tag, "No saved button names found - using defaults");
        for (int i = 0; i < 6; i++) {
            strncpy(button_names[i], default_button_names[i], 11);
        }
    } else if (ret != ESP_OK) {
        ESP_LOGE(s_tag, "Error reading button names: %s", esp_err_to_name(ret));
        for (int i = 0; i < 6; i++) {
            strncpy(button_names[i], default_button_names[i], 11);
        }
    } else {
        ESP_LOGI(s_tag, "Button names loaded from NVS successfully");
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
    ESP_LOGI(s_tag, "=== SAVE OPERATION STARTED ===");
    
    char button_names[6][11];
    bool save_names_success = false;
    
    
    // Button isimlerini al
    for (int i = 0; i < 6; i++) {
        const char *text = lv_textarea_get_text(settingsPage.button_name_textareas[i]);
        if (strlen(text) > 0) {
            strncpy(button_names[i], text, 10);
            button_names[i][10] = '\0';
        } else {
            strncpy(button_names[i], default_button_names[i], 11);
        }
        ESP_LOGI(s_tag, "Button %d name: '%s'", i + 1, button_names[i]);
    }
    
    // NVS'e kaydet
    ESP_LOGI(s_tag, "Saving to NVS...");
    
    // Button isimlerini kaydet
    esp_err_t ret_names = save_button_names_to_nvs(button_names);
    save_names_success = (ret_names == ESP_OK);
    
    if (save_names_success) {
        ESP_LOGI(s_tag, "Save results: Names=SUCCESS");
        lv_label_set_text(settingsPage.status_label, "Button names saved successfully!");
        lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        settingsPage.is_modified = false;
    } else {
        ESP_LOGI(s_tag, "Save results: Names=FAILED");
        
        lv_label_set_text(settingsPage.status_label, "Failed to save button names!");
        lv_obj_set_style_text_color(settingsPage.status_label, lv_palette_main(LV_PALETTE_RED), 0);
    }

    ESP_LOGI(s_tag, "=== SAVE OPERATION COMPLETED ===");
}

// Reset button callback
static void reset_button_cb(lv_event_t *e)
{
    ESP_LOGI(s_tag, "Reset button pressed");
    
    // Reset all textareas to default values
    for (int i = 0; i < 6; i++) {
        lv_textarea_set_text(settingsPage.button_name_textareas[i], default_button_names[i]);
    }
    
    lv_label_set_text(settingsPage.status_label, "Button names reset to defaults");
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
        for (int i = 0; i < 6; i++) {
            if (target == settingsPage.button_name_textareas[i] || 
                target == settingsPage.button_containers[i]) {
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

// Create unified button widget
static void create_unified_button_widget(int button_index, int row, int col)
{
    int container_width = 175;
    int container_height = 95;
    int spacing_x = 30;
    int spacing_y = 20;
    int start_x = 65;
    int start_y = 80;
    
    int x = start_x + col * (container_width + spacing_x);
    int y = start_y + row * (container_height + spacing_y);
    
    // Create main container
    settingsPage.button_containers[button_index] = lv_obj_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.button_containers[button_index], container_width, container_height);
    lv_obj_set_pos(settingsPage.button_containers[button_index], x, y);
    lv_obj_set_style_bg_color(settingsPage.button_containers[button_index], lv_color_make(20, 20, 20), 0);
    lv_obj_set_style_border_color(settingsPage.button_containers[button_index], lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_border_width(settingsPage.button_containers[button_index], 1, 0);
    lv_obj_set_style_radius(settingsPage.button_containers[button_index], 5, 0);
    lv_obj_set_style_pad_all(settingsPage.button_containers[button_index], 8, 0);
    
    
    // Create button name label
    settingsPage.button_name_labels[button_index] = lv_label_create(settingsPage.button_containers[button_index]);
    lv_label_set_text_fmt(settingsPage.button_name_labels[button_index], "Button %d Name:", button_index + 1);
    lv_obj_set_style_text_font(settingsPage.button_name_labels[button_index], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settingsPage.button_name_labels[button_index], color_primary, 0);
    lv_obj_set_pos(settingsPage.button_name_labels[button_index], 5, 5);
    
    // Create textarea for button name
    settingsPage.button_name_textareas[button_index] = lv_textarea_create(settingsPage.button_containers[button_index]);
    lv_obj_set_size(settingsPage.button_name_textareas[button_index], container_width - 21, 35);
    lv_obj_set_pos(settingsPage.button_name_textareas[button_index], 0, 30);
    lv_textarea_set_one_line(settingsPage.button_name_textareas[button_index], true);
    lv_textarea_set_max_length(settingsPage.button_name_textareas[button_index], 10);
    lv_textarea_set_placeholder_text(settingsPage.button_name_textareas[button_index], "Button name");
    
    // Style textarea
    lv_obj_set_style_bg_color(settingsPage.button_name_textareas[button_index], lv_color_make(40, 40, 40), 0);
    lv_obj_set_style_text_color(settingsPage.button_name_textareas[button_index], lv_color_white(), 0);
    lv_obj_set_style_border_color(settingsPage.button_name_textareas[button_index], lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_border_width(settingsPage.button_name_textareas[button_index], 1, 0);
    lv_obj_set_style_radius(settingsPage.button_name_textareas[button_index], 3, 0);
    
    // Add event callback
    lv_obj_add_event_cb(settingsPage.button_name_textareas[button_index], textarea_focus_cb, LV_EVENT_FOCUSED, NULL);

}



// Initialize settings page
void usrButtonSettingsPage_init(lv_obj_t *parent)
{
    if (settingsPage.page_container != NULL) {
        ESP_LOGW(s_tag, "Button settings page already initialized");
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
    lv_obj_set_pos(settingsPage.back_button, 65, 5);
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
    lv_label_set_text(settingsPage.title_label, "BUTTON SETTINGS");
    lv_obj_set_style_text_font(settingsPage.title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(settingsPage.title_label, color_primary, 0);
    lv_obj_set_pos(settingsPage.title_label, 260, 15);
    
    
    
    // Create unified button widgets (3x2 grid)
    for (int i = 0; i < 6; i++) {
        int row = i / 3;
        int col = i % 3;
        create_unified_button_widget(i, row, col);
    }
    
    // Create Save button
    settingsPage.save_button = lv_btn_create(settingsPage.page_container);
    lv_obj_set_size(settingsPage.save_button, 120, 45);
    lv_obj_set_pos(settingsPage.save_button, 230, 330);
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
    lv_obj_set_pos(settingsPage.reset_button, 365, 330);
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
    lv_label_set_text(settingsPage.status_label, "Modify button names and selection, then press Save All");
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
    lv_obj_set_content_height(settingsPage.page_container, 460);
    
    // Initialize state
    settingsPage.is_active = false;
    settingsPage.is_modified = false;
    settingsPage.current_textarea = NULL;
    settingsPage.back_callback = NULL;
    
    // Load button names and set them
    char button_names[6][11];
    load_button_names_from_nvs(button_names);
    for (int i = 0; i < 6; i++) {
        lv_textarea_set_text(settingsPage.button_name_textareas[i], button_names[i]);
    }
    
    // Hide page initially
    usrButtonSettingsPage_hide();
    
    ESP_LOGI(s_tag, "Button settings page initialized with NVS data loaded");
}

// Show settings page
void usrButtonSettingsPage_show(void)
{
    if (settingsPage.page_container != NULL) {
        usrGraphicalInterface_hideNavbar(); 
        lv_obj_clear_flag(settingsPage.page_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_scroll_to_y(settingsPage.page_container, 0, LV_ANIM_OFF);
        settingsPage.is_active = true;
        
        // Refresh from NVS and update checkboxes
        ESP_LOGI(s_tag, "Settings page shown - refreshing from NVS and updating checkboxes");
        
        
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
void usrButtonSettingsPage_hide(void)
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
void usrButtonSettingsPage_destroy(void)
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
        
        memset(&settingsPage, 0, sizeof(usrButtonSettingsPage_t));
        ESP_LOGI(s_tag, "Settings page destroyed and navbar restored");
    }
}

// Check if settings page is active
bool usrButtonSettingsPage_isActive(void)
{
    return settingsPage.is_active;
}

// Set back callback
void usrButtonSettingsPage_setBackCallback(void (*callback)(void))
{
    settingsPage.back_callback = callback;
}

// Load button names (for external use)
void usrButtonSettingsPage_loadButtonNames(char button_names[6][11])
{
    load_button_names_from_nvs(button_names);
}

// Save button names (for external use)
void usrButtonSettingsPage_saveButtonNames(void)
{
    save_button_cb(NULL);
}

