add_library(libcore STATIC
    profile.cpp profile.h
    linkparser.cpp linkparser.h
    subscription.cpp subscription.h
    configgen.cpp configgen.h
    masking.cpp masking.h
    geoip.cpp geoip.h
    protocol.h
)
set_target_properties(libcore PROPERTIES OUTPUT_NAME rlcore)
target_include_directories(libcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libcore PUBLIC Qt6::Core Qt6::Network)
