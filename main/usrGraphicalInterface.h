#ifndef _USR_GRAPHICAL_INTERFACE_H
#define _USR_GRAPHICAL_INTERFACE_H

#include "lvgl.h"
#include "usrGeneral.h"

// Include all page headers
#include "usrButtonPage.h"
#include "usrBatteryMonitorPage.h"
#include "usrTankLevelPage.h"
#include "usrShuntPage.h"

#include "marine_logo.h"

// Page enumeration
typedef enum
{
    PAGE_MAIN_MENU = 0,
    PAGE_BUTTON_CONTROL,
    PAGE_BATTERY_MONITOR,
    PAGE_TANK_LEVEL,
    PAGE_SHUNT_MONITOR,
    PAGE_COUNT = 4
} page_type_t;

// Main interface structure
typedef struct
{
    lv_obj_t *main_container;
    lv_obj_t *navigation_bar;
    lv_obj_t *nav_buttons[PAGE_COUNT];
    lv_obj_t *nav_labels[PAGE_COUNT];
    lv_obj_t *content_area;
    lv_obj_t *status_bar;
    lv_obj_t *time_label;
    lv_obj_t *system_status_label;
    page_type_t current_page;
    bool is_initialized;
    lv_timer_t *time_timer;
} usrGraphicalInterface_t;

// Function declarations
void usrGraphicalInterface_init(void);
void usrGraphicalInterface_destroy(void);
void usrGraphicalInterface_showPage(page_type_t page);
void usrGraphicalInterface_updateSystemStatus(const char *status);
bool usrGraphicalInterface_isInitialized(void);

void usrGraphicalInterface_hideNavbar(void);
void usrGraphicalInterface_showNavbar(void);

void buttonMenu(void);

void usrGraphicalInterface_debugSplash(void);
void usrGraphicalInterface_setLogoImage(const lv_img_dsc_t *logo_dsc);

#endif /* _USR_GRAPHICAL_INTERFACE_H */