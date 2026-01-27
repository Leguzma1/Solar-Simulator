#ifndef POWER_CONTROL_H
#define POWER_CONTROL_H
#endif

#include <stdint.h>
#include "esp_http_server.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "power_control.h"
#include "esp_netif.h"

// HTTP_handlers
esp_err_t led_on_handler(httpd_req_t *req);
esp_err_t led_off_handler(httpd_req_t *req);
// void configure_gpio(int8_t gpio, gpio_mode_t gpio_mode);
esp_err_t root_get_handler(httpd_req_t *req);
httpd_handle_t start_webserver(void);
void wifi_init_sta(void);