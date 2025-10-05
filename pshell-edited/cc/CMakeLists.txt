add_library(cc INTERFACE)
target_include_directories(cc INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Ensure Pico SDK and math are enabled for maximum feature set
target_compile_definitions(cc INTERFACE
    PICO_ON_DEVICE=1
    PICO_STDIO_UART=1
    PICO_STDIO_USB=1
    PICO_STDIO_SEMIHOSTING=1
    PICO_FLOAT_SUPPORT=1
    PICO_SDK_VERSION_MAJOR=1
    PICO_RP2040=1
)

target_sources(cc INTERFACE
    cc.c cc.h cc_tokns.h cc_ops.h
    cc_defs.h cc_extrns.h
    cc_malloc.c cc_malloc.h
    cc_peep.c cc_peep.h
    cc_printf.S 
)