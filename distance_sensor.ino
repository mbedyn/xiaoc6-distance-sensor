#include <Wire.h>
#include "VL53L1X.h"

#include "esp_zigbee_core.h"
#include "esp_zigbee_zcl.h"
#include "esp_zigbee_attribute.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_endpoint.h"
#include "esp_zigbee_device.h"

#define ZIGBEE_MODE ZIGBEE_MODE_ED
#define CUSTOM_CLUSTER_ID 0xFC00
#define CUSTOM_ATTR_DISTANCE 0x0001

// -----------------------------
// Zmienne pomiarowe
// -----------------------------
uint16_t distance_mm = 0;
uint16_t last_distance_mm = 0;
const uint16_t threshold_mm = 10;   // raportowanie przy zmianie > 10 mm

VL53L1X sensor;

// -----------------------------
// Custom cluster – definicja atrybutów
// -----------------------------
static esp_zb_zcl_attr_t custom_cluster_attrs[] = {
    {
        .id = CUSTOM_ATTR_DISTANCE,
        .type = ESP_ZB_ZCL_ATTR_TYPE_U16,
        .access = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
        .value = &distance_mm
    }
};

// Zmieniona funkcja: dodaje atrybuty bezpośrednio do istniejącej listy
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
// VL53L1X – inicjalizacja
// -----------------------------
void init_vl53()
{
    Wire.begin();

    sensor.setTimeout(500);
    if (!sensor.init()) {
        Serial.println("VL53L1X init failed!");
        while (1) { delay(1000); } // Zapobiega resetom Watchdoga
    }

    sensor.setDistanceMode(VL53L1X::Short);   // do ~1 m
    sensor.setMeasurementTimingBudget(20000); // 20 ms

    sensor.setROISize(8, 8);      // zawężenie wiązki do ~15°
    sensor.setROICenter(199);     // środek matrycy

    sensor.startContinuous(50);   // pomiar co 50 ms
}

// -----------------------------
// Raportowanie Zigbee
// -----------------------------
void report_distance(uint16_t mm)
{
    esp_zb_zcl_status_t status = esp_zb_zcl_set_attribute_val(
        1,
        CUSTOM_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        CUSTOM_ATTR_DISTANCE,
        &mm,
        false
    );

    if (status == ESP_ZB_ZCL_STATUS_SUCCESS) {
        esp_zb_zcl_report_attr(1, CUSTOM_CLUSTER_ID, CUSTOM_ATTR_DISTANCE);
    }
}

// -----------------------------
// Endpoint + clustery
// -----------------------------
static esp_zb_ep_list_t *create_endpoint()
{
    esp_zb_ep_list_t *ep = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    // Basic
    esp_zb_zcl_basic_cluster_add(cluster_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Identify
    esp_zb_zcl_identify_cluster_add(cluster_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    // Custom cluster - użycie poprawionej funkcji
    add_custom_cluster(cluster_list);

    // Endpoint 1
    esp_zb_ep_list_add_ep(
        ep,
        cluster_list,
        1,
        ESP_ZB_AF_HA_PROFILE_ID,
        ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID
    );

    return ep;
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

    esp_zb_ep_list_t *ep = create_endpoint();
    esp_zb_device_register(ep);

    esp_zb_start(true);
}

// -----------------------------
// LOOP
// -----------------------------
void loop()
{
    // Odczyt nieblokujący - chroni stos Zigbee i Watchdoga
    if (sensor.dataReady()) {
        uint16_t new_mm = sensor.read(false); 

        if (new_mm > 1000) new_mm = 1000;  // limit 1 m

        if (abs((int)new_mm - (int)last_distance_mm) >= threshold_mm) {
            distance_mm = new_mm;
            report_distance(distance_mm);
            last_distance_mm = new_mm;
            Serial.printf("Reported distance: %u mm\n", distance_mm);
        }
    }

    esp_zb_main_loop_iteration();
}
