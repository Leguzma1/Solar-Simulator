#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

/* WiFi configuration can be set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_STATION_EXAMPLE_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "SolarSim";

static int s_retry_num = 0;

/* Define GPIO numbers */
#define WHITE_GPIO 3

static const char *white_gpio_state = "off";

static httpd_handle_t server = NULL;

/* Build the HTML page */
static void build_html(char *buf, size_t buf_len)
{
    snprintf(buf, buf_len,
        "<!DOCTYPE html><html><head>"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<link rel=\"icon\" href=\"data:,\">"
        "<style>html { font-family: Helvetica; display: inline-block; "
        "margin: 0px auto; text-align: center;}"
        ".button { background-color: #4CAF50; border: none; color: white; "
        "padding: 16px 40px; text-decoration: none; font-size: 30px; "
        "margin: 2px; cursor: pointer;}"
        ".button2 { background-color: #555555; }</style></head>"
        "<body><h1>ESP32 Web Server</h1>"
        "<p>White LED - State %s</p>",
        white_gpio_state);

    if (strcmp(white_gpio_state, "off") == 0) {
        strlcat(buf, "<p><a href=\"/white/on\"><button class=\"button\">ON</button></a></p>", buf_len);
    } else {
        strlcat(buf, "<p><a href=\"/white/off\"><button class=\"button button2\">OFF</button></a></p>", buf_len);
    }

    // char tmp[256];
    // snprintf(tmp, sizeof(tmp),
    //     "<p>GPIO 27 - State %s</p>",
    //     output27_state);
    // strlcat(buf, tmp, buf_len);

    // if (strcmp(output27_state, "off") == 0) {
    //     strlcat(buf, "<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>", buf_len);
    // } else {
    //     strlcat(buf, "<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>", buf_len);
    // }

    strlcat(buf, "</body></html>", buf_len);
}

/* HTTP handlers */

static esp_err_t root_get_handler(httpd_req_t *req)
{
    char resp[1024];
    build_html(resp, sizeof(resp));
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t gpio_white_on_handler(httpd_req_t *req)
{
    white_gpio_state = "on";
    gpio_set_level(WHITE_GPIO, 1);
    return root_get_handler(req);
}

static esp_err_t gpio_white_off_handler(httpd_req_t *req)
{
    white_gpio_state = "off";
    gpio_set_level(WHITE_GPIO, 0);
    return root_get_handler(req);
}

// static esp_err_t gpio27_on_handler(httpd_req_t *req)
// {
//     output27_state = "on";
//     gpio_set_level(OUTPUT27, 1);
//     return root_get_handler(req);
// }

// static esp_err_t gpio27_off_handler(httpd_req_t *req)
// {
//     output27_state = "off";
//     gpio_set_level(OUTPUT27, 0);
//     return root_get_handler(req);
// }

/* Start HTTP server and register URIs */
static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {

        httpd_uri_t root_uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t gpio26_on_uri = {
            .uri      = "/white/on",
            .method   = HTTP_GET,
            .handler  = gpio_white_on_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &gpio26_on_uri);

        httpd_uri_t gpio26_off_uri = {
            .uri      = "/white/off",
            .method   = HTTP_GET,
            .handler  = gpio_white_off_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &gpio26_off_uri);

        // httpd_uri_t gpio27_on_uri = {
        //     .uri      = "/27/on",
        //     .method   = HTTP_GET,
        //     .handler  = gpio27_on_handler,
        //     .user_ctx = NULL
        // };
        // httpd_register_uri_handler(server, &gpio27_on_uri);

        // httpd_uri_t gpio27_off_uri = {
        //     .uri      = "/27/off",
        //     .method   = HTTP_GET,
        //     .handler  = gpio27_off_handler,
        //     .user_ctx = NULL
        // };
        // httpd_register_uri_handler(server, &gpio27_off_uri);

        ESP_LOGI(TAG, "HTTP server started");
    }
    return server;
}
/////////////////

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

static void configure_gpio(int8_t gpio, gpio_mode_t gpio_mode)
{
    ESP_LOGI(TAG, "Example configured GPIO %d!", gpio);
    gpio_reset_pin(gpio);
    /* Set the GPIO as a push/pull output */
    // gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(gpio, gpio_mode);
}

void app_main(void)
{
    /* Configure the GPIOs*/
    configure_gpio(WHITE_GPIO, GPIO_MODE_OUTPUT);
    // configure_led(RED_GPIO, GPIO_MODE_OUTPUT);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    // no loop() needed; handlers run in esp-idf tasks
    server = start_webserver(); // Start HTTP server
}