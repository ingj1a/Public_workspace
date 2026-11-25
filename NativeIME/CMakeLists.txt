cmake_minimum_required(VERSION 3.25)
project(NativeIME VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nativeime_core
    src/core/pinyin_engine.cpp
)
target_include_directories(nativeime_core PUBLIC src)
target_compile_features(nativeime_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(nativeime_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(nativeime_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(nativeime_core_tests tests/core_tests.cpp)
target_link_libraries(nativeime_core_tests PRIVATE nativeime_core)

enable_testing()
add_test(NAME nativeime_core_tests COMMAND nativeime_core_tests)

if(WIN32)
    add_library(nativeime_tsf SHARED
        src/windows/tsf_dll.cpp
        src/windows/NativeIME.def
    )
    target_compile_definitions(nativeime_tsf PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(nativeime_tsf PRIVATE ole32 advapi32 uuid)
    set_target_properties(nativeime_tsf PROPERTIES
        OUTPUT_NAME "NativeIME.TSF"
        CXX_VISIBILITY_PRESET hidden
    )
endif()
