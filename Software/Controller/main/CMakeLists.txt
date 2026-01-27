idf_component_register(SRCS "SolarSimulatorController.c" 
                    PRIV_REQUIRES esp_wifi nvs_flash esp_http_server html_helper dac39j81q MCP4802
                    INCLUDE_DIRS ".")
