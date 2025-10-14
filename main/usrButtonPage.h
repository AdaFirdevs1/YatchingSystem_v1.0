#ifndef _USR_BUTTON_PAGE_H_
#define _USR_BUTTON_PAGE_H_

#include "lvgl.h"
#include "usrGeneral.h"

typedef enum
{
    BUTTON_1_PRESSED = 1,
    BUTTON_2_PRESSED,
    BUTTON_3_PRESSED,
    BUTTON_4_PRESSED,
    BUTTON_5_PRESSED,
    BUTTON_6_PRESSED
} button_event_t;

typedef struct
{
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *settings_button;
    lv_obj_t *buttons[6];
    lv_obj_t *button_labels[6];
    lv_obj_t *pwm_sliders[6];
    lv_obj_t *slider_labels[6];
    lv_obj_t *status_label;
    bool button_states[6];
    uint32_t slider_values[6];
    bool is_active;

     // New circular elements
    lv_obj_t *arc_meters[6];           // Arc meters for PWM control
    lv_obj_t *center_buttons[6];       // Center clickable buttons
    lv_obj_t *center_button_labels[6]; // Button names in center
    lv_obj_t *value_labels[6];         // Value display below arcs
    
} usrButtonPage_t;

void usrButtonPage_init(lv_obj_t *parent);
void usrButtonPage_show(void);
void usrButtonPage_hide(void);
void usrButtonPage_destroy(void);
void usrButtonPage_setStatus(const char *status);
bool usrButtonPage_isActive(void);

void usrButtonPage_button1_handler(void);
void usrButtonPage_button2_handler(void);
void usrButtonPage_button3_handler(void);
void usrButtonPage_button4_handler(void);
void usrButtonPage_button5_handler(void);
void usrButtonPage_button6_handler(void);

bool usrButtonPage_getButtonState(int channel);
uint32_t usrButtonPage_getSliderValue(int channel);


esp_err_t save_button_states_to_nvs(void);
esp_err_t load_button_states_from_nvs(void);
esp_err_t save_slider_values_to_nvs(void);
esp_err_t load_slider_values_from_nvs(void);

#endif /* _USR_BUTTON_PAGE_H_ */