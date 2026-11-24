cmake_minimum_required(VERSION 3.21)

project(PolyScope
    VERSION 0.1.0
    DESCRIPTION "Display and control Android devices with ADB and scrcpy"
    LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX) # window activation helper (AppKit)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(POLYSCOPE_BUILD_TESTS "Build unit tests" ON)

set(POLYSCOPE_QT_COMPONENTS Core Gui Qml Quick QuickControls2 Network Svg)
if(POLYSCOPE_BUILD_TESTS)
    list(APPEND POLYSCOPE_QT_COMPONENTS Test)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS ${POLYSCOPE_QT_COMPONENTS})
qt_standard_project_setup(REQUIRES 6.8)

function(polyscope_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---- Pure logic: no QProcess/QML, fully unit-testable -----------------------
qt_add_library(polyscope_pure STATIC
    src/adb/AdbParsers.cpp
    src/adb/AdbParsers.h
    src/adb/MonitorParsers.cpp
    src/adb/MonitorParsers.h
    src/adb/CommandCatalog.cpp
    src/adb/CommandCatalog.h
    src/core/ToolSearch.cpp
    src/core/ToolSearch.h
    src/scrcpy/ScrcpyArgs.cpp
    src/scrcpy/ScrcpyArgs.h
)
target_include_directories(polyscope_pure PUBLIC src)
target_link_libraries(polyscope_pure PUBLIC Qt6::Core)
polyscope_warnings(polyscope_pure)

# ---- Application ------------------------------------------------------------
set(APP_ICON_SOURCES)
if(APPLE)
    set(MACOSX_ICON ${CMAKE_SOURCE_DIR}/packaging/macos/PolyScope.icns)
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND APP_ICON_SOURCES ${MACOSX_ICON})
elseif(WIN32)
    list(APPEND APP_ICON_SOURCES packaging/windows/PolyScope.rc)
endif()

qt_add_executable(PolyScope MACOSX_BUNDLE WIN32 src/main.cpp ${APP_ICON_SOURCES})

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(PolyScope
    URI PolyScope
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/AppButton.qml
        qml/BarRow.qml
        qml/DashboardPage.qml
        qml/DonutChart.qml
        qml/Gauge.qml
        qml/LineChart.qml
        qml/StatTile.qml
        qml/AppCheckBox.qml
        qml/AppDialog.qml
        qml/AppTabButton.qml
        qml/AppWindow.qml
        qml/TextWindow.qml
        qml/AppsPage.qml
        qml/Badge.qml
        qml/Card.qml
        qml/CommandForm.qml
        qml/CommandGroupPage.qml
        qml/CommandPalette.qml
        qml/ControlPage.qml
        qml/EmptyState.qml
        qml/FilesPage.qml
        qml/InfoPage.qml
        qml/LogsPage.qml
        qml/MirrorPage.qml
        qml/PageTabs.qml
        qml/ParamField.qml
        qml/PathField.qml
        qml/SectionTitle.qml
        qml/ServicesPage.qml
        qml/SettingsDialog.qml
        qml/ShellPage.qml
        qml/Toast.qml
        qml/ToolBanner.qml
        qml/WirelessDialog.qml
        qml/WrappedLabel.qml
    SOURCES
        src/adb/AdbClient.cpp
        src/adb/AdbClient.h
        src/adb/AdbTracker.cpp
        src/adb/AdbTracker.h
        src/adb/ProcessRunner.cpp
        src/adb/ProcessRunner.h
        src/core/AppContext.cpp
        src/core/AppContext.h
        src/core/AppSettings.cpp
        src/core/AppSettings.h
        src/core/Logging.cpp
        src/core/Logging.h
        src/core/SmokeTest.cpp
        src/core/WindowActivation.cpp
        src/core/WindowActivation.h
        src/core/SmokeTest.h
        src/core/ToolPaths.cpp
        src/core/ToolPaths.h
        src/adb/CommandRunner.cpp
        src/adb/CommandRunner.h
        src/models/DeviceModel.cpp
        src/models/DeviceModel.h
        src/models/MdnsModel.cpp
        src/models/MdnsModel.h
        src/scrcpy/ScrcpyManager.cpp
        src/scrcpy/ScrcpyManager.h
        src/tools/DeviceActions.cpp
        src/tools/DeviceMonitor.cpp
        src/tools/DeviceMonitor.h
        src/tools/StaleResult.h
        src/tools/DeviceActions.h
        src/tools/FileBrowserModel.cpp
        src/tools/FileBrowserModel.h
        src/tools/LogcatModel.cpp
        src/tools/LogcatModel.h
        src/tools/PackageModel.cpp
        src/tools/PackageModel.h
        src/tools/ServicesModel.cpp
        src/tools/ServicesModel.h
        src/tools/ShellConsole.cpp
        src/tools/ShellConsole.h
)

qt_add_resources(PolyScope polyscope_icons
    PREFIX /icons
    BASE resources/icons
    FILES resources/icons/app-256.png
)

# qmltyperegistrar includes headers by basename, so every source directory must be on the path.
target_include_directories(PolyScope PRIVATE
    src src/adb src/core src/models src/scrcpy src/tools)
target_compile_definitions(PolyScope PRIVATE
    POLYSCOPE_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_FOREACH)
target_link_libraries(PolyScope PRIVATE
    polyscope_pure
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Svg)
polyscope_warnings(PolyScope)

if(APPLE)
    target_sources(PolyScope PRIVATE src/core/WindowActivation_mac.mm)
    target_link_libraries(PolyScope PRIVATE "-framework AppKit")
endif()

set_target_properties(PolyScope PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "PolyScope"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.polyscope.PolyScope"
    MACOSX_BUNDLE_ICON_FILE "PolyScope.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "MIT License"
    WIN32_EXECUTABLE TRUE)

# ---- Install (Linux desktop integration) ------------------------------------
include(GNUInstallDirs)
install(TARGETS PolyScope BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/polyscope.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    foreach(sz 16 32 48 64 128 256 512)
        install(FILES packaging/linux/icons/${sz}x${sz}/polyscope.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${sz}x${sz}/apps)
    endforeach()
endif()

# ---- Tests ------------------------------------------------------------------
if(POLYSCOPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
