cmake_minimum_required(VERSION 3.16)

project(mx-arch-updater VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus Svg)

# Enable Qt6 automoc, autorcc, autouic
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Add compile definitions for Qt6 compatibility
add_compile_definitions(QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII QT_NO_NARROWING_CONVERSIONS)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Source files
set(COMMON_SOURCES
    src/common.cpp
)

set(SETTINGS_SOURCES
    src/settings_main.cpp
    src/settings_dialog.cpp
    src/settings_service.cpp
)

set(MONITOR_SOURCES
    src/monitor_main.cpp
    src/system_monitor.cpp
)

set(SYSTRAY_SOURCES
    src/systray_main.cpp
    src/tray_app.cpp
    src/tray_service.cpp
)

set(VIEW_SOURCES
    src/view_main.cpp
    src/view_and_upgrade.cpp
)

# Create executables
add_executable(updater-settings ${SETTINGS_SOURCES} ${COMMON_SOURCES})
add_executable(updater-system-monitor ${MONITOR_SOURCES} ${COMMON_SOURCES})
add_executable(updater-systray ${SYSTRAY_SOURCES} ${COMMON_SOURCES})
add_executable(updater-view-and-upgrade ${VIEW_SOURCES} ${COMMON_SOURCES})

# Link Qt libraries
target_link_libraries(updater-settings Qt6::Core Qt6::Widgets Qt6::DBus Qt6::Svg)
target_link_libraries(updater-system-monitor Qt6::Core Qt6::DBus)
target_link_libraries(updater-systray Qt6::Core Qt6::Widgets Qt6::DBus Qt6::Svg)
target_link_libraries(updater-view-and-upgrade Qt6::Core Qt6::Widgets Qt6::DBus Qt6::Svg)

# Installation
install(TARGETS updater-settings updater-system-monitor updater-systray updater-view-and-upgrade
    RUNTIME DESTINATION bin
)

# Install data files
install(DIRECTORY icons DESTINATION share/mx-arch-updater)
# D-Bus system service and policy
install(FILES dbus/org.mxlinux.UpdaterSystemMonitor.service DESTINATION share/dbus-1/system-services)
install(FILES dbus/org.mxlinux.UpdaterSystemMonitor.conf DESTINATION share/dbus-1/system.d)
# D-Bus session services
install(FILES dbus/org.mxlinux.UpdaterSystemTrayIcon.service DESTINATION share/dbus-1/services)
install(FILES dbus/org.mxlinux.UpdaterSettings.service DESTINATION share/dbus-1/services)
install(DIRECTORY polkit DESTINATION share/polkit-1/actions)
install(FILES systemd/mx-arch-updater-monitor.service DESTINATION lib/systemd/system)
install(FILES systemd/mx-arch-updater-tray.service DESTINATION lib/systemd/user)
install(FILES README.md DESTINATION share/doc/mx-arch-updater)