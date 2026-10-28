cmake_minimum_required(VERSION 3.21)
project(superpupervpn VERSION 0.2.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SUPERPUPERVPN_BUILD_GUI "Build the Qt Quick GUI" ON)
option(SUPERPUPERVPN_BUNDLE_QT "Install the Qt it was built with into lib/superpupervpn, for distros whose Qt is too old" OFF)
option(SUPERPUPERVPN_DEV "Developer build: env overrides for paths/socket, unpinned core allowed, screenshot hook. Never for packages" OFF)
set(SUPERPUPERVPN_CORE_SHA256 "" CACHE STRING "SHA-256 of the pinned mihomo binary, baked into the helper (НФТ-БЕЗ-05)")
set(SUPERPUPERVPN_GITHUB_REPO "NikitaKnutov/superpupervpn" CACHE STRING "owner/repo checked for new releases (ФТ-35)")

set(RL_QT_COMPONENTS Core Network Test LinguistTools)
if(SUPERPUPERVPN_BUILD_GUI)
    list(APPEND RL_QT_COMPONENTS Gui Qml Quick QuickControls2 Widgets)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND RL_QT_COMPONENTS DBus)
endif()
if(SUPERPUPERVPN_BUNDLE_QT)
    # A private dir, so the bundled Qt never replaces or clashes with the system one.
    set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib/superpupervpn/lib)
    # plugins/ and qml/ keep Qt's layout next to lib/, so their shipped RUNPATHs stay right (read by find_package).
    set(QT_DEPLOY_FORCE_ADJUST_RPATHS OFF)
endif()
find_package(Qt6 6.8 REQUIRED COMPONENTS ${RL_QT_COMPONENTS})
include(GNUInstallDirs)
if(WIN32)
    set(SUPERPUPERVPN_HELPER_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
else()
    set(SUPERPUPERVPN_HELPER_INSTALL_DIR ${CMAKE_INSTALL_LIBEXECDIR}/superpupervpn)
endif()
qt_standard_project_setup(REQUIRES 6.8)

add_compile_definitions(SUPERPUPERVPN_VERSION="${PROJECT_VERSION}")
if(MSVC)
    add_compile_options(/utf-8)
endif()
if(SUPERPUPERVPN_DEV)
    add_compile_definitions(SUPERPUPERVPN_DEV)
endif()

add_subdirectory(src/core)
add_subdirectory(src/helper)
if(SUPERPUPERVPN_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

enable_testing()
add_subdirectory(tests)

# English is the source language; the Russian .qm is embedded into both binaries as :/i18n/superpupervpn_ru.qm.
# After changing strings: cmake --build build --target update_translations
set(RL_TR_TARGETS superpupervpn-helper)
if(SUPERPUPERVPN_BUILD_GUI)
    list(APPEND RL_TR_TARGETS superpupervpn)
endif()
# The other platform's files are not part of any target here, but their strings still belong in the .ts.
file(GLOB RL_TR_PLATFORM_SOURCES CONFIGURE_DEPENDS src/*/*_win.cpp src/*/*_linux.cpp)
qt_add_translations(TARGETS ${RL_TR_TARGETS} tst_core
    SOURCE_TARGETS libcore ${RL_TR_TARGETS}
    SOURCES ${RL_TR_PLATFORM_SOURCES}
    TS_FILES translations/superpupervpn_ru.ts
    LUPDATE_OPTIONS -no-obsolete -locations relative)
