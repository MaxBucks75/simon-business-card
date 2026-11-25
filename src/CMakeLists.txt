# Collect all .c files in this directory
file(GLOB SRC_FILES *.c)

# Create a library from them
add_library(ece315_src ${SRC_FILES})

# Optionally include headers from the include directory
target_include_directories(ece315_src PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include)


# Link against the Pico SDK
target_link_libraries(ece315_src pico_stdlib 
                      hardware_spi 
                      hardware_uart 
                      hardware_gpio 
                      hardware_pwm)
