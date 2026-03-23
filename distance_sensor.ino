#include <Wire.h>
#include "VL53L1X.h"

#include "esp_zigbee_core.h"
#include "esp_zigbee_zcl.h"
#include "esp_zigbee_attribute.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_endpoint.h"
#include "esp_zigbee_device.h"

// -----------------------------
// Konfiguracja Zigbee
// -----------------------------
#define ZIGBEE_MODE             ZIGBEE_MODE_ED
#define CUSTOM_CLUSTER_ID       0xFC00
#define CUSTOM_ATTR_DISTANCE    0x0001

// -----------------------------
// Konfiguracja VL53L1X
// -----------------------------
#define VL53L1X_TIMING_BUDGET_US    20000   // 20 ms – balans między prędkością a dokładnością
#define VL53L1X_POLL_INTERVAL_MS    50      // co 50 ms nowy pomiar
#define VL53L1X_MAX_DISTANCE_MM     1000    // limit trybu Short (~1 m)
#define VL53L1X_ROI_SIZE            8       // zawężenie wiązki SPAD do ~15°
#define VL53L1X_ROI_CENTER          199     // środek matrycy SPAD (centrum optyczne)
#define REPORT_THRESHOLD_MM         10      // min. zmiana do wysłania raportu Zigbee

// -----------------------------
// Zmienne pomiarowe
// -----------------------------
static uint16_t distance_mm      = 0;
static uint16_t last_distance_mm = 0;

VL53L1X sensor;

// -----------------------------
// Custom cluster – definicja atrybutów
// -----------------------------
static esp_zb_zcl_attr_t custom_cluster_attrs[] = {
    {
        .id     = CUSTOM_ATTR_DISTANCE,
        .type   = ESP_ZB_ZCL_ATTR_TYPE_U16,
        .access = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
        .value  = &distance_mm   // wskaźnik na globalną zmienną – zawsze aktualizuj distance_mm PRZED raportem
    }
};

static void add_custom_cluster(esp_zb_cluster_list_t *cluster_list)
{
    esp_zb_zcl_cluster_add_custom_cluster(
        cluster_list,
        CUSTOM_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        custom_cluster_attrs,
        sizeof(custom_cluster_attrs) / sizeof(custom_cluster_attrs[0])
    );
}

// -----------------------------
// Konfiguracja automatycznego raportowania atrybutu
// -----------------------------
static void configure_reporting()
{
    esp_zb_zcl_reporting_info_t rep_info = {
        .direction    = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep           = 1,
        .cluster_id   = CUSTOM_CLUSTER_ID,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .attr_id      = CUSTOM_ATTR_DISTANCE,
        .u            = {
            .send_info = {
                .min_interval = 1,      // min 1 s między raportami
                .max_interval = 60,     // wymuś raport co 60 s nawet bez zmiany
                .delta        = { .u16 = REPORT_THRESHOLD_MM },
                .def_min_interval = 1,
                .def_max_interval = 60
            }
        },
        .dst = {
            .profile_id = ESP_ZB_AF_HA_PROFILE_ID
        },
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC
    };

    esp_zb_update_reporting_info(&rep_info);
}

// -----------------------------
// Handler zdarzeń Zigbee (join, leave, rejoin)
// -----------------------------
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    switch (callback_id) {
        case ESP_ZB_CORE_NETWORK_STEERING_CB_ID:
            Serial.println("[Zigbee] Network steering complete");
            configure_reporting();  // konfiguruj reporting po dołączeniu do sieci
            break;
        case ESP_ZB_CORE_DEVICE_CB_ID:
            Serial.println("[Zigbee] Device callback");
            break;
        default:
            Serial.printf("[Zigbee] Unhandled callback: %d\n", callback_id);
            break;
    }
    return ESP_OK;
}

// -----------------------------
// Raportowanie Zigbee (z blokadą stosu)
// -----------------------------
static void report_distance(uint16_t mm)
{
    // Aktualizuj globalną zmienną wskazywaną przez atrybut
    distance_mm = mm;

    esp_zb_lock_acquire(portMAX_DELAY);

    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        1,
        CUSTOM_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        CUSTOM_ATTR_DISTANCE,
        &distance_mm,
        false
    );

    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        esp_zb_zcl_report_attr(1, CUSTOM_CLUSTER_ID, CUSTOM_ATTR_DISTANCE);
    } else {
        Serial.printf("[Zigbee] set_attribute_val failed: %d\n", status);
    }

    esp_zb_lock_release();
}

// -----------------------------
// VL53L1X – inicjalizacja
// -----------------------------
static void init_vl53()
{
    Wire.begin();

    sensor.setTimeout(500);
    if (!sensor.init()) {
        Serial.println("[VL53L1X] Init failed! Restarting...");
        delay(2000);
        esp_restart();  // twardy reset zamiast pętli blokującej
    }

    sensor.setDistanceMode(VL53L1X::Short);
    sensor.setMeasurementTimingBudget(VL53L1X_TIMING_BUDGET_US);
    sensor.setROISize(VL53L1X_ROI_SIZE, VL53L1X_ROI_SIZE);
    sensor.setROICenter(VL53L1X_ROI_CENTER);
    sensor.startContinuous(VL53L1X_POLL_INTERVAL_MS);

    Serial.println("[VL53L1X] Initialized OK");
}

// -----------------------------
// Endpoint + clustery
// -----------------------------
static esp_zb_ep_list_t *create_endpoint()
{
    esp_zb_ep_list_t *ep_list          = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    esp_zb_zcl_basic_cluster_add(cluster_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_zcl_identify_cluster_add(cluster_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    add_custom_cluster(cluster_list);

    esp_zb_ep_list_add_ep(
        ep_list,
        cluster_list,
        1,
        ESP_ZB_AF_HA_PROFILE_ID,
        ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID
    );

    return ep_list;
}

// -----------------------------
// SETUP
// -----------------------------
void setup()
{
    Serial.begin(115200);
    init_vl53();

    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    // Rejestracja handlera zdarzeń – musi być przed esp_zb_start()
    esp_zb_core_action_handler_register(zb_action_handler);

    esp_zb_ep_list_t *ep = create_endpoint();
    esp_zb_device_register(ep);

    esp_zb_start(true);
    Serial.println("[Zigbee] Stack started");
}

// -----------------------------
// LOOP
// -----------------------------
void loop()
{
    if (sensor.dataReady()) {
        uint16_t new_mm = sensor.read(false);  // false = nie resetuj licznika timeoutu

        // Sprawdź timeout sensora
        if (sensor.timeoutOccurred()) {
            Serial.println("[VL53L1X] Timeout!");
            esp_zb_main_loop_iteration();
            return;
        }

        // Ogranicz do zakresu trybu Short
        if (new_mm > VL53L1X_MAX_DISTANCE_MM) {
            new_mm = VL53L1X_MAX_DISTANCE_MM;
        }

        // Wyślij raport tylko przy wystarczającej zmianie odległości
        uint16_t diff = (new_mm > last_distance_mm)
                        ? new_mm - last_distance_mm
                        : last_distance_mm - new_mm;

        if (diff >= REPORT_THRESHOLD_MM) {
            report_distance(new_mm);
            last_distance_mm = new_mm;
            Serial.printf("[VL53L1X] Reported distance: %u mm\n", new_mm);
        }
    }

    esp_zb_main_loop_iteration();
}
