cmake_minimum_required(VERSION 3.21)

project(pic-viewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)
include(CTest)

if(APPLE)
    set(WrapOpenGL_AGL "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/OpenGL.framework" CACHE FILEPATH "Compatibility shim for Qt WrapOpenGL on modern macOS" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent Test)
find_package(JPEG REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBHEIF REQUIRED IMPORTED_TARGET libheif)
pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw)

qt_standard_project_setup()

if(APPLE)
    get_target_property(QT_GUI_LINK_LIBS Qt6::Gui INTERFACE_LINK_LIBRARIES)
    if(QT_GUI_LINK_LIBS)
        list(REMOVE_ITEM QT_GUI_LINK_LIBS "AGL")
        list(REMOVE_ITEM QT_GUI_LINK_LIBS "-framework AGL")
        set_target_properties(Qt6::Gui PROPERTIES INTERFACE_LINK_LIBRARIES "${QT_GUI_LINK_LIBS}")
    endif()
endif()

set(PIC_VIEWER_SOURCES
    assets/resources.qrc
    src/app/main.cpp
    src/app/main_window.cpp
    src/catalog/image_catalog.cpp
    src/core/image_cache.cpp
    src/core/prefetch_scheduler.cpp
    src/core/slide_show_controller.cpp
    src/decoder/image_decoder.cpp
    src/viewer/image_viewer_widget.cpp
)

set(PIC_VIEWER_HEADERS
    src/app/main_window.h
    src/catalog/image_catalog.h
    src/core/image_cache.h
    src/core/image_types.h
    src/core/prefetch_scheduler.h
    src/core/slide_show_controller.h
    src/decoder/image_decoder.h
    src/viewer/image_viewer_widget.h
)

qt_add_executable(pic-viewer
    WIN32
    MANUAL_FINALIZATION
    ${PIC_VIEWER_SOURCES}
    ${PIC_VIEWER_HEADERS}
)

target_link_libraries(pic-viewer
    PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
        JPEG::JPEG
        PkgConfig::LIBHEIF
        PkgConfig::LIBRAW
)

target_include_directories(pic-viewer PRIVATE src)

qt_add_executable(test_image_catalog
    tests/test_image_catalog.cpp
    src/catalog/image_catalog.cpp
)

qt_add_executable(test_slide_show_controller
    tests/test_slide_show_controller.cpp
    src/core/slide_show_controller.cpp
)

qt_add_executable(test_prefetch_scheduler
    tests/test_prefetch_scheduler.cpp
    src/core/prefetch_scheduler.cpp
)

qt_add_executable(test_image_decoder
    tests/test_image_decoder.cpp
    src/decoder/image_decoder.cpp
)

target_link_libraries(test_image_catalog
    PRIVATE
        Qt6::Test
        Qt6::Widgets
)

target_link_libraries(test_slide_show_controller
    PRIVATE
        Qt6::Test
        Qt6::Widgets
)

target_link_libraries(test_prefetch_scheduler
    PRIVATE
        Qt6::Test
        Qt6::Widgets
)

target_link_libraries(test_image_decoder
    PRIVATE
        Qt6::Test
        Qt6::Widgets
        JPEG::JPEG
        PkgConfig::LIBHEIF
        PkgConfig::LIBRAW
)

target_include_directories(test_image_catalog PRIVATE src)
target_include_directories(test_slide_show_controller PRIVATE src)
target_include_directories(test_prefetch_scheduler PRIVATE src)
target_include_directories(test_image_decoder PRIVATE src)

install(TARGETS pic-viewer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(pic-viewer)

add_test(NAME test_image_catalog COMMAND test_image_catalog)
add_test(NAME test_slide_show_controller COMMAND test_slide_show_controller)
add_test(NAME test_prefetch_scheduler COMMAND test_prefetch_scheduler)
add_test(NAME test_image_decoder COMMAND test_image_decoder)
