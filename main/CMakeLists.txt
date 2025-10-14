idf_component_register(
    SRCS 
        "main.c"
        "usrCAN.c"
        "usrGraphicalInterface.c"
        "usrButtonPage.c"
        "usrBatteryMonitorPage.c" 
        "waveshare_rgb_lcd_port.c"
        "usrTankLevelPage.c"
        "usrShuntPage.c"
        "lvgl_port.c"
        "usrTankSettingsPage.c"
        "usrDefinePassword.c"
        "usrTankPasswordPage.c" 
        "usrMainPage.c"
        "usrButtonSettingsPage.c"
        "usrButtonPasswordPage.c"
        "usrBatterySettingsPage.c"
        "usrBatteryPasswordPage.c"
        "usrShuntSettingsPage.c"
        "usrShuntPasswordPage.c"
        "marine_logo.c"
    INCLUDE_DIRS ".")

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)