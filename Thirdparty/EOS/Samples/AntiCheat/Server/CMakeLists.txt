#CMAKE file for build
project (AntiCheatServer)

cmake_minimum_required(VERSION 3.10.2)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release) #default type is release
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include_directories ("${PROJECT_SOURCE_DIR}/Source")

#EOS SDK location
if(DEV)
	set(EOS_SDK_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../../../SDK/Public")
	if(APPLE)
		set(EOS_SDK_BIN_DIR "${PROJECT_SOURCE_DIR}/../../../../../Binaries/Mac")
	elseif(UNIX)
		set(EOS_SDK_BIN_DIR "${PROJECT_SOURCE_DIR}/../../../../../Binaries/Linux")
	endif (APPLE)
else (DEV)
	set(EOS_SDK_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../../../SDK/Include")
	set(EOS_SDK_BIN_DIR "${PROJECT_SOURCE_DIR}/../../../SDK/Bin")
endif (DEV)

if(APPLE)
	set(EOS_SDK_LIBRARIES "${EOS_SDK_BIN_DIR}/libEOSSDK-Mac-Shipping.dylib") #mac
elseif(UNIX)
	set(EOS_SDK_LIBRARIES "${EOS_SDK_BIN_DIR}/libEOSSDK-Linux-Shipping.so") #linux
endif(APPLE)

include_directories ("${PROJECT_SOURCE_DIR}/Source")
include_directories ("${PROJECT_SOURCE_DIR}/Source/Main")
include_directories ("${PROJECT_SOURCE_DIR}/../../Shared/Source/Utils")
include_directories ("${PROJECT_SOURCE_DIR}/../../Shared/External")
include_directories ("${PROJECT_SOURCE_DIR}/../../Shared/External/SDL2/Include")
include_directories ("${PROJECT_SOURCE_DIR}/../../Shared/External/SDL2/SDL2_net/include")
include_directories ("${PROJECT_SOURCE_DIR}/../../Shared/External/UTF8-CPP/source")
include_directories ("${EOS_SDK_INCLUDE_DIR}")

if(DEV)
	include_directories("${PROJECT_SOURCE_DIR}/../../Shared/NotForLicensees/Source/Core")
	add_definitions(-DALLOW_RESERVED_PLATFORM_OPTIONS)
endif(DEV)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(THIRD_PARTY_DEPENDENCY_LIBS "")

#Search for dependencies
#SDL
find_package (SDL2 REQUIRED)
find_package (SDL2NET REQUIRED)
if (NOT SDL2_FOUND)
	message (STATUS "SDL not found!")
elseif (NOT SDL2NET_FOUND)
	message (STATUS "SDL NET not found!")
else ()
	include_directories(${SDL2_INCLUDE_DIR})
	include_directories(${SDL2NET_INCLUDE_DIR})
endif()
if(SDL2_FOUND AND SDL2NET_FOUND)
	set(THIRD_PARTY_DEPENDENCY_LIBS
		${SDL2_LIBRARY}
		${SDL2NET_LIBRARY}
	)
message (STATUS "All 3rd parties found: ${THIRD_PARTY_DEPENDENCY_LIBS}")
endif()

set(SOURCES
	../../Shared/Source/Utils/CommandLine.cpp
	../../Shared/Source/Utils/DebugLog.cpp
	../../Shared/Source/Utils/StringUtils.cpp
	../../Shared/Source/Utils/Settings.cpp
	../../Shared/Source/Utils/Utils.cpp

	Source/Main/Main.cpp
	Source/Main/ServerMain.cpp

	Source/pch.cpp
	Source/AntiCheatNetworkTransport.cpp
	Source/AntiCheatServer.cpp
	Source/EosSdk.cpp
	Source/TCPClient.cpp
)

if(APPLE)
	list(APPEND SOURCES
		Source/Main/MacMain.mm
		Source/Main/MacMain.h
	)
endif(APPLE)

if(APPLE)
	# set how it shows up in the Info.plist file
	set(MACOSX_BUNDLE_ICON_FILE Game.icns) 
	# set where in the bundle to put the icns file
	set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Source/Game.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	# include the icns file in the target
	set(APPICONS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Game.icns)
	
	set(SYSTEM_LIBS "-framework Foundation" "-framework Cocoa")
endif(APPLE)

set(APP_NAME AntiCheatServer)

find_package(Threads REQUIRED)
if(THREADS_HAVE_PTHREAD_ARG)
  set_property(TARGET my_app PROPERTY COMPILE_OPTIONS "-pthread")
  set_property(TARGET my_app PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()

add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${APPICONS})

target_link_libraries(${APP_NAME} ${SYSTEM_LIBS} ${THIRD_PARTY_DEPENDENCY_LIBS} ${EOS_SDK_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

set_property(TARGET ${APP_NAME} PROPERTY C_STANDARD 99)
set_property(TARGET ${APP_NAME} PROPERTY CXX_STANDARD 14)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Destination paths below are relative to ${CMAKE_INSTALL_PREFIX}
install(TARGETS ${APP_NAME}
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime)

# Set platform-specific path to executable
set(APPS "\${CMAKE_INSTALL_PREFIX}/bin/${APP_NAME}")
if(APPLE)
	set(APPS "\${CMAKE_INSTALL_PREFIX}/${APP_NAME}.app")
elseif(WIN32)
	set(APPS "\${CMAKE_INSTALL_PREFIX}/bin/${APP_NAME}.exe")
endif(APPLE)

# Directories to look in for dependencies
set(DIRS ${EOS_SDK_BIN_DIR}
		 ${SDL2_LIBRARY}
		 ${SDL2NET_LIBRARY}
	)

install(CODE "include(BundleUtilities)
    fixup_bundle(\"${APPS}\" \"\" \"${DIRS}\")")
