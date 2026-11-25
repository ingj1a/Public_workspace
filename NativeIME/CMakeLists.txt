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

add_executable(nativeime_session_tests tests/session_tests.cpp)
target_sources(nativeime_core PRIVATE src/core/input_session.cpp)
target_link_libraries(nativeime_session_tests PRIVATE nativeime_core)
if(MSVC)
    add_compile_options(/utf-8)
    target_compile_options(nativeime_core PRIVATE /utf-8)
    target_compile_options(nativeime_core_tests PRIVATE /utf-8)
    target_compile_options(nativeime_session_tests PRIVATE /utf-8)
endif()

enable_testing()
add_test(NAME nativeime_core_tests COMMAND nativeime_core_tests)

add_test(NAME nativeime_session_tests COMMAND nativeime_session_tests)

option(NATIVEIME_EXPERIMENTAL_TSF "Enable unvalidated inline TSF composition for lab only" OFF)
if(WIN32)
    add_library(nativeime_tsf SHARED
        src/windows/tsf_dll.cpp
        src/windows/tsf_edit.cpp
        src/windows/NativeIME.def
    )
    target_compile_definitions(nativeime_tsf PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(nativeime_tsf PRIVATE ole32 advapi32 uuid user32)
    if(NATIVEIME_EXPERIMENTAL_TSF)
        target_compile_definitions(nativeime_tsf PRIVATE NATIVEIME_EXPERIMENTAL_TSF)
        target_link_libraries(nativeime_tsf PRIVATE nativeime_core)
    endif()
    add_executable(nativeime_tsf_smoke tests/tsf_smoke.cpp)
    target_link_libraries(nativeime_tsf_smoke PRIVATE ole32 uuid)
    add_test(NAME nativeime_tsf_smoke COMMAND nativeime_tsf_smoke $<TARGET_FILE:nativeime_tsf>)
    add_executable(NativeIME.InputLab WIN32 src/windows/preview.cpp)
    target_compile_definitions(NativeIME.InputLab PRIVATE UNICODE _UNICODE)
    target_link_libraries(NativeIME.InputLab PRIVATE nativeime_core user32 gdi32)
    set_target_properties(nativeime_tsf PROPERTIES
        OUTPUT_NAME "NativeIME.TSF"
        CXX_VISIBILITY_PRESET hidden
    )
endif()
