
add_executable(master
    "main.c"
    "uart.c"
    "rgb.c"
)

target_link_libraries(master shared)
