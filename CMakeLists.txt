cmake_minimum_required(VERSION 3.21)
project(snapline VERSION 0.2.0 DESCRIPTION "Region screenshot tool with annotation and upload" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

option(SNAPLINE_X11 "Enable X11 global hotkeys and cursor capture" ON)
option(BUILD_TESTING "Build unit tests" ON)

set(SNAPLINE_APP_ID "io.github.snapline.Snapline")

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Gui Network DBus PrintSupport Svg)

if(SNAPLINE_X11)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11DEPS REQUIRED IMPORTED_TARGET xcb xcb-xfixes x11)
endif()

add_library(snapline_core STATIC
    src/core/Hotkeys.cpp
    src/core/JsonPath.cpp
    src/core/FileNameTemplate.cpp
    src/capture/Capture.cpp
    src/tools/Annotation.cpp
    src/tools/AnnotationStack.cpp
)
target_include_directories(snapline_core PUBLIC src)
target_link_libraries(snapline_core PUBLIC Qt6::Gui)

add_library(snapline_app STATIC
    src/app/Application.cpp
    src/app/Clipboard.cpp
    src/app/Config.cpp
    src/app/GnomeShortcut.cpp
    src/app/History.cpp
    src/app/HotkeyManager.cpp
    src/app/ImageExport.cpp
    src/app/Notifier.cpp
    src/app/PortalRequest.cpp
    src/app/SettingsDialog.cpp
    src/app/SingleInstance.cpp
    src/capture/ScreenGrabber.cpp
    src/overlay/CaptureController.cpp
    src/overlay/CaptureSession.cpp
    src/overlay/OverlayWindow.cpp
    src/overlay/Toolbars.cpp
    src/upload/Uploader.cpp
    src/upload/UploadDialog.cpp
)
target_compile_definitions(snapline_app PUBLIC
    SNAPLINE_VERSION="${PROJECT_VERSION}"
    SNAPLINE_APP_ID="${SNAPLINE_APP_ID}"
)
target_link_libraries(snapline_app PUBLIC
    snapline_core
    Qt6::Widgets Qt6::Network Qt6::DBus Qt6::PrintSupport Qt6::Svg
)
if(SNAPLINE_X11)
    target_compile_definitions(snapline_app PRIVATE SNAPLINE_HAVE_X11)
    target_link_libraries(snapline_app PRIVATE PkgConfig::X11DEPS)
endif()

# Resources go into the executables: static libraries drop unreferenced resource initializers.
add_executable(snapline src/main.cpp resources/resources.qrc)
target_link_libraries(snapline PRIVATE snapline_app)

install(TARGETS snapline RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/${SNAPLINE_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/${SNAPLINE_APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES resources/icons/snapline.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME ${SNAPLINE_APP_ID}.svg)

if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE snapline_core Qt6::Test)
    add_test(NAME test_core COMMAND test_core)
    set_tests_properties(test_core PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(test_overlay tests/test_overlay.cpp resources/resources.qrc)
    target_link_libraries(test_overlay PRIVATE snapline_app Qt6::Test)
    add_test(NAME test_overlay COMMAND test_overlay)
    set_tests_properties(test_overlay PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
