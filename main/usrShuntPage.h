#ifndef _USR_SHUNT_PAGE_H_
#define _USR_SHUNT_PAGE_H_

#include "lvgl.h"
#include "usrGeneral.h"

// Shunt tipi
typedef enum
{
    SHUNT_TYPE_MAIN = 0,
    SHUNT_TYPE_QUADRO = 1
} shunt_type_t;

// Her bir shunt kanalı için veri yapısı
typedef struct
{
    float current;           // Anlık akım (amper) - negatif değer discharge, pozitif charge
    float power;             // Güç (watt)
    float voltage;           // Voltaj (volt)
    float resistance_mohm;   // Shunt direnci (mOhm)
    float max_current;       // Maksimum akım kapasitesi
    bool is_connected;       // Bağlantı durumu
    uint32_t last_data_time; // Son veri alma zamanı
    char name[16];           // Shunt adı
} shunt_data_t;

// Batarya veri yapısı
typedef struct
{
    float voltage;           // Batarya voltajı (gerçek voltaj - gerilim bölücü düzeltilmiş)
    float min_voltage;       // Minimum voltaj (boş batarya - 10.5V for 12V lead-acid)
    float max_voltage;       // Maksimum voltaj (dolu batarya - 14.4V for 12V lead-acid)
    bool is_connected;       // Bağlantı durumu
    uint32_t last_data_time; // Son veri alma zamanı
    char name[16];           // Batarya adı
} shunt_battery_data_t;

// Sayfa yapısı
typedef struct
{
    lv_obj_t *page_container;
    lv_obj_t *title_label;
    lv_obj_t *status_label;

    // Main Shunt (yatay - 300A) - STM32'den gelen shunt akımı
    lv_obj_t *main_shunt_container;
    lv_obj_t *main_shunt_bar;
    lv_obj_t *main_shunt_label;
    lv_obj_t *main_shunt_value_label;

    // Battery Arc Graphics (2 adet) - STM32'den gelen batarya voltajları
    lv_obj_t *battery_containers[2]; // Ana batarya ve yedek batarya
    lv_obj_t *battery_arcs[2];
    lv_obj_t *battery_labels[2];
    lv_obj_t *battery_voltage_labels[2];
    lv_obj_t *battery_percentage_labels[2];

    // Quadro Shunts (4 dikey - 30A her biri) - İsteğe bağlı ek shuntlar
    lv_obj_t *quadro_containers[4];
    lv_obj_t *quadro_bars[4];
    lv_obj_t *quadro_labels[4];
    lv_obj_t *quadro_value_labels[4];

    // Data structures
    shunt_data_t main_shunt;           // Ana shunt verisi (STM32'den)
    shunt_battery_data_t batteries[2]; // Batarya verileri (STM32'den)
    shunt_data_t quadro_shunts[4];     // Ek shunt verileri

    // Control variables
    bool is_active;
    lv_timer_t *update_timer;
    bool getShuntRequest;

    lv_obj_t *settings_button;
    char custom_shunt_names[5][16];  // Özelleştirilmiş shunt isimleri
    bool selected_shunts[5];         // Hangi shuntların görüntüleneceği
    
} usrShuntPage_t;

// Fonksiyon bildirimleri
void usrShuntPage_init(lv_obj_t *parent);
void usrShuntPage_show(void);
void usrShuntPage_hide(void);
void usrShuntPage_destroy(void);
void usrShuntPage_setStatus(const char *status);

// CAN veri işleme - STM32 ile uyumlu
void usrShuntPage_processCanData(uint32_t can_id, uint32_t data);                                  // Legacy 32-bit format
void usrShuntPage_processCanDataExtended(uint32_t can_id, uint8_t *can_data, uint8_t data_length); // Extended 8-byte format
void usrShuntPage_updateShuntData(int shunt_index, shunt_type_t shunt_type, float current, float voltage);
void usrShuntPage_updateBatteryData(int battery_index, float voltage);

// UI güncelleme
void usrShuntPage_updateMainShuntDisplay(void);
void usrShuntPage_updateQuadroShuntDisplay(int quadro_index);
void usrShuntPage_updateBatteryDisplay(int battery_index);

// Veri toplama kontrol
void usrShuntPage_startDataCollection(void);
void usrShuntPage_stopDataCollection(void);
bool usrShuntPage_isActive(void);

#endif /* _USR_SHUNT_PAGE_H_ */