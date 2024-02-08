cmake_minimum_required(VERSION 3.24)

option(USE_QML "Create QML types and modules" OFF)
include(${CMAKE_CURRENT_BINARY_DIR}/local_conf.cmake OPTIONAL)


include(FetchContent)
FetchContent_Declare(
    ccommon
    GIT_REPOSITORY https://github.com/EddyTheCo/Common.git
    GIT_TAG v0.1.1
)
FetchContent_MakeAvailable(ccommon)
version_from_git(
    LOG       OFF
    TIMESTAMP "%Y%m%d%H%M%S"
)

project(IotaWallet VERSION ${VERSION} DESCRIPTION "Wallet methods for Iota " LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
include(GNUInstallDirs)

FetchContent_Declare(
    IotaAddressWallet
    GIT_REPOSITORY https://github.com/EddyTheCo/QAddrBundle.git
    GIT_TAG v0.4.0
    FIND_PACKAGE_ARGS 0.4 CONFIG
)
FetchContent_MakeAvailable(IotaAddressWallet)


if(USE_QML)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick)
else()
    find_package(Qt6 COMPONENTS Core)
endif()

if (Qt6_FOUND)
    qt_standard_project_setup()
    add_library(wallet src/qwallet.cpp include/qwallet.hpp)
    if(USE_QML)
        qt6_add_qml_module(wallet
            URI  Esterv.Iota.Wallet
            VERSION 1.0
            RESOURCE_PREFIX
            "/esterVtech.com/imports"
            OUTPUT_TARGETS out_targets_var
            OUTPUT_DIRECTORY
            ${CMAKE_BINARY_DIR}/Esterv/Iota/Wallet
            IMPORT_PATH ${CMAKE_BINARY_DIR}
        )


    add_library(${PROJECT_NAME}::walletplugin ALIAS walletplugin)

    target_link_libraries(wallet PUBLIC Qt6::Gui Qt6::Quick)
    target_compile_definitions(wallet PRIVATE USE_QML)
    target_link_libraries(wallet PUBLIC
        $<$<STREQUAL:$<TARGET_PROPERTY:IotaAddressWallet::addrBundle,TYPE>,STATIC_LIBRARY>:IotaAddressWallet::addrBundleplugin>
    )
    install(TARGETS ${out_targets_var}
        EXPORT ${PROJECT_NAME}-config
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT wallet
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Esterv/Iota/Wallet
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/Esterv/Iota
        COMPONENT wallet
    )
    install(TARGETS walletplugin
        EXPORT ${PROJECT_NAME}-config
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/Esterv/Iota/Wallet
        COMPONENT wallet
    )
    endif()
    add_library(${PROJECT_NAME}::wallet ALIAS wallet)
    target_compile_definitions(wallet PRIVATE WINDOWS_QWALLET)
    target_link_libraries(wallet PUBLIC  Qt6::Core)
    target_link_libraries(wallet PUBLIC  IotaAddressWallet::addrBundle)

target_include_directories(wallet PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                 "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Esterv/${PROJECT_NAME}>")

    install(TARGETS wallet
        EXPORT ${PROJECT_NAME}-config
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT wallet
    )
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Esterv/${PROJECT_NAME}
    COMPONENT wallet
)
install(EXPORT ${PROJECT_NAME}-config
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    NAMESPACE ${PROJECT_NAME}::
    COMPONENT ${PROJECT_NAME}
)
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
export(EXPORT ${PROJECT_NAME}-config
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CPACK_PACKAGE_CONTACT "estervtech")
    include(CTest)
    add_subdirectory(examples)
    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-v${SEMVER}-${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_VERSION}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}")
    if(NOT BUILD_SHARED_LIBS)
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-static")
    endif(NOT BUILD_SHARED_LIBS)
    if(USE_THREADS)
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-MThread")
    endif(USE_THREADS)
    include(CPack)
endif()
endif(Qt6_FOUND)

if(BUILD_DOCS)
    get_target_property(build_docs cmake_build_docs SOURCES)
    include(${build_docs})
endif()
