idf_component_register(
    SRCS "main.c" "pwm.c" "console_io.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_ledc esp_driver_usb_serial_jtag
)