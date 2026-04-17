project(main)

add_executable(main
    source/main.cpp
)
target_link_libraries(main PUBLIC emulator)

target_compile_options(main PRIVATE
    "--embed-dir=${CMAKE_SOURCE_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    target_compile_options(main PRIVATE
        "-fexceptions"
        "-pthread"
    )
    target_link_options(main PRIVATE
        "-sWASM=1"
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sENVIRONMENT=web"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
        "-sEXPORTED_FUNCTIONS=_getOutputQueue,_startEmulator"
        "-sINITIAL_MEMORY=2147483648"
        "-sMAXIMUM_MEMORY=2147483648"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sSTACK_SIZE=1048576"
        "-sASSERTIONS=1"
        "-sPTHREAD_POOL_SIZE=5"
    )

    # Produces emulator.js and emulator.wasm for frontend imports.
    set_target_properties(main PROPERTIES OUTPUT_NAME "emulator")
endif()