cmake_minimum_required(VERSION 3.16)

# Use APP_VERSION from command line if provided, otherwise read from PKGBUILD
if(NOT APP_VERSION)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/PKGBUILD")
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/PKGBUILD" PKGBUILD_CONTENT)
        string(REGEX MATCH "pkgver=([^\n]+)" _ ${PKGBUILD_CONTENT})
        set(PKGBUILD_PKGVER ${CMAKE_MATCH_1})
        string(REGEX MATCH "pkgrel=([^\n]+)" _ ${PKGBUILD_CONTENT})
        set(PKGBUILD_PKGREL ${CMAKE_MATCH_1})
        set(APP_VERSION "${PKGBUILD_PKGVER}-${PKGBUILD_PKGREL}")
    else()
        set(APP_VERSION "unknown")
    endif()
endif()

project(update-notifier-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus Svg)

# Enable Qt6 automoc, autorcc, autouic
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Add compile definitions for Qt6 compatibility
add_compile_definitions(QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_NARROWING_CONVERSIONS)

# Generate version header from PKGBUILD
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Source files
set(COMMON_SOURCES
    src/common.cpp
)

set(MONITOR_SOURCES
    src/monitor_main.cpp
    src/system_monitor.cpp
)

set(SYSTRAY_SOURCES
    src/systray_main.cpp
    src/tray_app.cpp
    src/tray_service.cpp
    src/settings_dialog.cpp
    src/settings_service.cpp
    src/history_dialog.cpp
)

set(VIEW_SOURCES
    src/view_main.cpp
    src/view_and_upgrade.cpp
)

# Create executables
add_executable(update-notifier-system-monitor ${MONITOR_SOURCES} ${COMMON_SOURCES})
add_executable(update-notifier-systray ${SYSTRAY_SOURCES} ${COMMON_SOURCES})
add_executable(update-notifier-view-and-upgrade ${VIEW_SOURCES} ${COMMON_SOURCES})

# Link Qt libraries
target_link_libraries(update-notifier-system-monitor Qt6::Core Qt6::DBus)
target_link_libraries(update-notifier-systray Qt6::Core Qt6::Widgets Qt6::DBus Qt6::Svg)
target_link_libraries(update-notifier-view-and-upgrade Qt6::Core Qt6::Widgets Qt6::DBus Qt6::Svg)

# Installation
install(TARGETS update-notifier-system-monitor update-notifier-systray update-notifier-view-and-upgrade
    RUNTIME DESTINATION bin
)

# Install data files
install(DIRECTORY icons DESTINATION share/update-notifier-qt)
# D-Bus system service and policy
install(FILES dbus/org.mxlinux.UpdateNotifierSystemMonitor.service DESTINATION share/dbus-1/system-services)
install(FILES dbus/org.mxlinux.UpdateNotifierSystemMonitor.conf DESTINATION share/dbus-1/system.d)
# D-Bus session service
install(FILES dbus/org.mxlinux.UpdateNotifierTrayIcon.service DESTINATION share/dbus-1/services)
install(FILES systemd/update-notifier-monitor.service DESTINATION lib/systemd/system)
install(FILES systemd/update-notifier-tray.service DESTINATION lib/systemd/user)
install(FILES systemd/50-update-notifier.preset DESTINATION lib/systemd/user-preset)
install(FILES README.md DESTINATION share/doc/update-notifier-qt)
