# cz-discputer: USB optical drive emulator (GPL-3.0). Built through cardputer-zero-tools:
#   cmake -S <cardputer-zero-tools> -DCZ_APP_DIR=<this folder> ...   (scripts/dp does it)

set(DP_VERSION 0.1.0)

add_library(dp_core STATIC
    core/image_info.cpp
    core/library.cpp
)
target_include_directories(dp_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core")

find_package(Threads REQUIRED)

add_executable(cz-discputer
    src/main.cpp
    src/gadget.cpp
    src/disc_app.cpp
)
target_link_libraries(cz-discputer PRIVATE czkit dp_core Threads::Threads)
target_compile_definitions(cz-discputer PRIVATE DP_DEV_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(USE_DESKTOP)
    add_executable(dp_core_tests tests/core_tests.cpp)
    target_link_libraries(dp_core_tests PRIVATE dp_core)
    add_test(NAME dp_core COMMAND dp_core_tests)
endif()

# the root helper and its sudo rule (written by postinst after a visudo check)
install(PROGRAMS packaging/dp-gadget DESTINATION libexec/discputer)

include("${PROJECT_SOURCE_DIR}/cmake/cz-package.cmake")
cz_package_app(
    TARGET       cz-discputer
    DISPLAY_NAME "DiscPuter"
    VERSION      ${DP_VERSION}
    SUMMARY      "USB optical drive emulator: boot installers from ISO images"
    ICON         assets/images/cz-discputer.png
    ASSETS_DIR   assets
    LICENSE      ../LICENSE
)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/prerm;${CMAKE_CURRENT_SOURCE_DIR}/packaging/postrm")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "sudo")
include(CPack)
