idf_component_register(SRCS "hello_world_main.c" "screen.c"
                       PRIV_REQUIRES spi_flash esp_driver_spi driver esp_lcd
                       INCLUDE_DIRS "")
