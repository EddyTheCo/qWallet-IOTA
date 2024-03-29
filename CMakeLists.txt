cmake_minimum_required(VERSION 3.24)

option(USE_QML "Create QML types and modules" ON)
include(${CMAKE_CURRENT_BINARY_DIR}/local_conf.cmake OPTIONAL)


include(FetchContent)
FetchContent_Declare(
	ccommon
	GIT_REPOSITORY https://github.com/EddyTheCo/Common.git
	GIT_TAG v0.3.0
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
	GIT_TAG v0.5.0
	FIND_PACKAGE_ARGS 0.5 CONFIG
	)
FetchContent_MakeAvailable(IotaAddressWallet)


if(USE_QML)
	find_package(Qt6 COMPONENTS Core Gui Quick)
else()
	find_package(Qt6 COMPONENTS Core)
endif()

if (Qt6_FOUND)
	qt_standard_project_setup()
	add_library(qwallet src/qwallet.cpp include/qwallet.hpp)
	set_target_properties(qwallet PROPERTIES VERSION ${VERSION} SOVERSION ${VERSION_MAJOR})
	if(USE_QML)
		qt6_add_qml_module(qwallet
			URI  Esterv.Iota.Wallet
			VERSION 1.0
			RESOURCE_PREFIX
			"/esterVtech.com/imports"
			OUTPUT_TARGETS out_targets_var
			OUTPUT_DIRECTORY
			${CMAKE_BINARY_DIR}/Esterv/Iota/Wallet
			IMPORT_PATH ${CMAKE_BINARY_DIR}
			)


		add_library(${PROJECT_NAME}::qwalletplugin ALIAS qwalletplugin)

		target_link_libraries(qwallet PUBLIC Qt6::Gui Qt6::Quick)
		target_compile_definitions(qwallet PRIVATE USE_QML)
		target_link_libraries(qwallet PUBLIC
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
		install(TARGETS qwalletplugin
			EXPORT ${PROJECT_NAME}-config
			DESTINATION ${CMAKE_INSTALL_LIBDIR}/Esterv/Iota/Wallet
			COMPONENT wallet
			)
	endif()
	add_library(${PROJECT_NAME}::qwallet ALIAS qwallet)
	target_compile_definitions(qwallet PRIVATE WINDOWS_QWALLET)
	target_link_libraries(qwallet PUBLIC  Qt6::Core)
	target_link_libraries(qwallet PUBLIC  IotaAddressWallet::addrBundle)

	target_include_directories(qwallet PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Esterv/>")

	install(TARGETS qwallet
		EXPORT ${PROJECT_NAME}-config
		DESTINATION ${CMAKE_INSTALL_LIBDIR}
		COMPONENT wallet
		)
	install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Esterv
		COMPONENT ${PROJECT_NAME}
		)
	build_exports()
	if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
		add_subdirectory(examples)
		build_cpack()
	endif(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
endif(Qt6_FOUND)

if(BUILD_DOCS)
	build_docs()
endif()
