idf_component_register(
    SRCS 
        "main.c"
        
    INCLUDE_DIRS "."
    REQUIRES 
        led
        middle
        driver
        espressif__led_strip
        nvs_flash
)
