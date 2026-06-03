idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp8266-tcp-transport
        log
        pthread
)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u pthread_include_pthread_cond_var_impl")
