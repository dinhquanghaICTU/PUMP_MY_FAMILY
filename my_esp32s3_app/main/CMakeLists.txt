idf_component_register(
    SRCS 
        "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        application
        hardware
        middle
        driver
        espressif__led_strip
        nvs_flash
)
