find_package(OpenSSL 3 REQUIRED)

qt_add_executable(superpupervpn
    main.cpp
    app.cpp app.h
    models.cpp models.h
    storage.cpp storage.h
    helperclient.cpp helperclient.h
    sysproxy.cpp sysproxy.h
    platformgui.h
    theme_tokens.h
)
if(WIN32)
    target_sources(superpupervpn PRIVATE keystore_win.cpp sysproxy_win.cpp platformgui_win.cpp)
    target_link_libraries(superpupervpn PRIVATE crypt32 wininet rasapi32 shell32)
    set_target_properties(superpupervpn PROPERTIES WIN32_EXECUTABLE ON)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
    target_sources(superpupervpn PRIVATE keystore_linux.cpp sysproxy_linux.cpp platformgui_linux.cpp)
    target_link_libraries(superpupervpn PRIVATE PkgConfig::LIBSECRET)
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(superpupervpn
    URI SuperPuperVPN
    VERSION 1.0
    NO_CACHEGEN
    QML_FILES
        qml/Main.qml qml/Theme.qml qml/HomePage.qml qml/ServersPage.qml qml/LogsPage.qml qml/SettingsPage.qml
        qml/ImportDialog.qml qml/ProfileDialog.qml qml/Onboarding.qml
        qml/Card.qml qml/DelayChip.qml qml/Segmented.qml qml/ConnectButton.qml qml/Tab.qml
        qml/PillButton.qml qml/Field.qml qml/Flag.qml qml/Sheet.qml
)
file(GLOB UI_ICONS ${PROJECT_SOURCE_DIR}/assets/icons/ui/*.svg)
qt_add_resources(superpupervpn assets
    PREFIX /
    BASE ${PROJECT_SOURCE_DIR}/assets
    FILES ${PROJECT_SOURCE_DIR}/assets/icons/superpupervpn.svg ${PROJECT_SOURCE_DIR}/assets/fonts/InterVariable.ttf
          ${PROJECT_SOURCE_DIR}/assets/geo/country.bin
          ${UI_ICONS}
)

target_compile_definitions(superpupervpn PRIVATE SUPERPUPERVPN_GITHUB_REPO="${SUPERPUPERVPN_GITHUB_REPO}")
target_link_libraries(superpupervpn PRIVATE libcore Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::Network OpenSSL::Crypto)
install(TARGETS superpupervpn RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(SUPERPUPERVPN_BUNDLE_QT)
    # Qt is relocatable: it finds plugins/ and qml/ next to the lib/ it was loaded from. This is
    # qt_generate_deploy_qml_app_script minus its qt.conf, which in /usr/bin would redirect every Qt app there.
    qt6_generate_deploy_script(TARGET superpupervpn OUTPUT_SCRIPT deploy_script CONTENT "
set(QT_DEPLOY_LIB_DIR lib/superpupervpn/lib)
set(QT_DEPLOY_PLUGINS_DIR lib/superpupervpn/plugins)
set(QT_DEPLOY_QML_DIR lib/superpupervpn/qml)
qt6_deploy_qml_imports(TARGET superpupervpn PLUGINS_FOUND plugins_found)
qt6_deploy_runtime_dependencies(EXECUTABLE $<TARGET_FILE:superpupervpn> ADDITIONAL_MODULES \${plugins_found} NO_TRANSLATIONS)")
    install(SCRIPT ${deploy_script})
endif()
