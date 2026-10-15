add_executable(superpupervpn-helper
    main.cpp
    helper.cpp helper.h
    coreprocess.cpp coreprocess.h
    platform.h
)
if(WIN32)
    target_sources(superpupervpn-helper PRIVATE platform_win.cpp)
    target_link_libraries(superpupervpn-helper PRIVATE iphlpapi advapi32)
else()
    target_sources(superpupervpn-helper PRIVATE platform_linux.cpp)
endif()
target_compile_definitions(superpupervpn-helper PRIVATE SUPERPUPERVPN_CORE_SHA256="${SUPERPUPERVPN_CORE_SHA256}")
target_link_libraries(superpupervpn-helper PRIVATE libcore Qt6::Core Qt6::Network)
install(TARGETS superpupervpn-helper DESTINATION ${SUPERPUPERVPN_HELPER_INSTALL_DIR})
