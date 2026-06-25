cmake_minimum_required(VERSION 3.16)
project(staffel_viper_readycheck VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# MSVC does not auto-export symbols — this makes CMake generate a .def file for all symbols
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Qt5 MSVC – same version and compiler as TS3 → uses TS3's own Qt5 DLLs at runtime
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)

# TS3 SDK headers
set(TS3_SDK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/ts3_sdk/include")

set(SOURCES
    src/plugin.cpp
    src/hud_window.cpp
    src/status_manager.cpp
    src/supabase_client.cpp
    src/hotkey_manager.cpp
    src/settings_dialog.cpp
)

set(HEADERS
    include/plugin.h
    include/hud_window.h
    include/status_manager.h
    include/supabase_client.h
    include/hotkey_manager.h
    include/member_status.h
    include/settings_dialog.h
)

add_library(${PROJECT_NAME} SHARED
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${TS3_SDK_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::WebSockets
    $<$<BOOL:${WIN32}>:user32>
    $<$<NOT:$<BOOL:${WIN32}>>:dl>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PLUGIN_VERSION="${PROJECT_VERSION}"
    QT_NO_KEYWORDS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "staffel_viper_readycheck_win64")
endif()

# ---- Packaging ----
# Qt5 DLLs are NOT bundled – TS3 already ships identical Qt5 DLLs (same MSVC build).
# Only our plugin DLL goes into the package.
set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")
file(MAKE_DIRECTORY "${PACKAGE_DIR}/plugins")
set(PACKAGE_NAME "staffel_viper_readycheck")

add_custom_target(package_ts3plugin
    DEPENDS ${PROJECT_NAME}
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${PACKAGE_DIR}/plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}/plugins"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${PACKAGE_DIR}/plugins/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/packaging/package.ini" "${PACKAGE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_BINARY_DIR}/${PACKAGE_NAME}.ts3_plugin" --format=zip
            "plugins" "package.ini"
    WORKING_DIRECTORY "${PACKAGE_DIR}"
    COMMENT "Creating ${PACKAGE_NAME}.ts3_plugin"
)
