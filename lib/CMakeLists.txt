project(emulator_lib)

add_library(emulator STATIC
        source/address_space.cpp
        source/riscv/core.cpp
)
target_include_directories(emulator PUBLIC include)
