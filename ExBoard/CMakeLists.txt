if(DEFINED CONFIG_SAMPLE_SUPPORT_ExBoard)
set(SOURCES_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/ExBoard_sample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pwm_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server_adv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/my_iic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bh1750.c
    ${CMAKE_CURRENT_SOURCE_DIR}/aht20.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.c
    ${CMAKE_CURRENT_SOURCE_DIR}/motor.c
)

set(PUBLIC_HEADER_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/pwm_ex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sle_uart_server_adv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/my_iic.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bh1750.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aht20.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.h
    ${CMAKE_CURRENT_SOURCE_DIR}/motor.h
)
set(SOURCES "${SOURCES}" ${SOURCES_LIST} PARENT_SCOPE)
endif()