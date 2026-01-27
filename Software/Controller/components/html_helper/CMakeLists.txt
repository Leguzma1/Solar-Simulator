idf_component_register(
    SRCS "html_helper.c"
    PRIV_REQUIRES esp_wifi esp_http_server 
    INCLUDE_DIRS "."
    REQUIRES power_control
)