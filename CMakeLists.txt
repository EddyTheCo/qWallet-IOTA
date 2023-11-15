cmake_minimum_required(VERSION 3.16)


include(${CMAKE_CURRENT_BINARY_DIR}/local_conf.cmake OPTIONAL)


include(FetchContent)
FetchContent_Declare(
	ccommon
	GIT_REPOSITORY https://github.com/EddyTheCo/Common.git
	GIT_TAG main
	)
FetchContent_MakeAvailable(ccommon)
version_from_git(
	LOG       OFF
	TIMESTAMP "%Y%m%d%H%M%S"
	)

project(IOTAWallet VERSION ${VERSION} LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
include(GNUInstallDirs)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
		STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
		"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

option(USE_QML "Create QML types and modules" OFF)


FetchContent_Declare(
	account
	GIT_REPOSITORY https://github.com/EddyTheCo/account.git
        GIT_TAG expose_qml_or_not
	FIND_PACKAGE_ARGS 0.4 CONFIG
	)
FetchContent_MakeAvailable(account)
FetchContent_Declare(
	nodeConection
	GIT_REPOSITORY https://github.com/EddyTheCo/ConectionSettings.git
        GIT_TAG expose_qml_or_not
	FIND_PACKAGE_ARGS 0.4 CONFIG
	)
FetchContent_MakeAvailable(nodeConection)
if(USE_QML)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick)
else()
    find_package(Qt6 COMPONENTS Core)
endif()

if (Qt6_FOUND)
	qt_standard_project_setup()
	add_library(qwallet src/qwallet.cpp include/wallet/qwallet.hpp)

	target_compile_definitions(qwallet PRIVATE WINDOWS_QWALLET)
	target_link_libraries(qwallet PUBLIC  Qt6::Core)
        target_link_libraries(qwallet PUBLIC  nodeConection account )

	target_include_directories(qwallet PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		"$<INSTALL_INTERFACE:include>")
	install(TARGETS qwallet EXPORT qwalletTargets DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

	install(EXPORT qwalletTargets
		FILE qwalletTargets.cmake
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qwallet
		)
	include(CMakePackageConfigHelpers)
	# generate the config file that is includes the exports
	configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
		"${CMAKE_CURRENT_BINARY_DIR}/qwalletConfig.cmake"
		INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/qwallet"
		)
	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/qwalletConfigVersion.cmake"
		VERSION ${VERSION}
		COMPATIBILITY SameMajorVersion
		)
	install(FILES
		${CMAKE_CURRENT_BINARY_DIR}/qwalletConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/qwalletConfigVersion.cmake
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/qwallet
		)
	export(EXPORT qwalletTargets
		FILE "${CMAKE_CURRENT_BINARY_DIR}/qwalletTargets.cmake"
		)


	if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
		set(CPACK_PACKAGE_CONTACT "estervtech")
		include(CTest)
		set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-v${SEMVER}-${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_VERSION}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}")
		if(NOT BUILD_SHARED_LIBS)
			set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-static")
		endif(NOT BUILD_SHARED_LIBS)
		include(CPack)
	endif()
endif(Qt6_FOUND)

if(BUILD_DOCS)
	get_target_property(build_docs cmake_build_docs SOURCES)
	include(${build_docs})
endif()
