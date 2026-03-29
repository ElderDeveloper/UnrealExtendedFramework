#CMAKE file for build

project (Mods)

cmake_minimum_required(VERSION 3.10.2)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release) #default type is release
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(BUILD_WITH_SDL "Do you want to use SDL?" TRUE)
if(BUILD_WITH_SDL)
    message(STATUS "We do build with SDL.")
    add_definitions(-DEOS_DEMO_SDL)
endif(BUILD_WITH_SDL)

option(BUILD_WITH_DXTK "Do you want to use DirectX Toolkit?" FALSE)
if(BUILD_WITH_DXTK)
    message(STATUS "We do build with DirectX Toolkit.")
    add_definitions(-DDXTK)
endif(BUILD_WITH_DXTK)

include_directories ("${PROJECT_SOURCE_DIR}/Source")

#EOS SDK location
if(DEV)
	set(EOS_SDK_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../../SDK/Public")
	if(APPLE)
		set(EOS_SDK_BIN_DIR "${PROJECT_SOURCE_DIR}/../../../../Binaries/Mac")
	elseif(UNIX)
		set(EOS_SDK_BIN_DIR "${PROJECT_SOURCE_DIR}/../../../../Binaries/Linux")
	endif (APPLE)
else (DEV)
	set(EOS_SDK_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../../SDK/Include")
	set(EOS_SDK_BIN_DIR "${PROJECT_SOURCE_DIR}/../../SDK/Bin")
endif (DEV)

if(APPLE)
	set(EOS_SDK_LIBRARIES "${EOS_SDK_BIN_DIR}/libEOSSDK-Mac-Shipping.dylib") #mac
elseif(UNIX)
	set(EOS_SDK_LIBRARIES "${EOS_SDK_BIN_DIR}/libEOSSDK-Linux-Shipping.so") #linux
endif(APPLE)

include_directories ("${PROJECT_SOURCE_DIR}/Source")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Core")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Graphics")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Graphics/GUI")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Input")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Utils")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Main")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Main/Windows")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/Source/Math")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/External/SDL2/Include")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/External/GLEW/include")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/External/SDL2/SDL2_ttf/include")
include_directories ("${PROJECT_SOURCE_DIR}/../Shared/External/UTF8-CPP/source")
include_directories ("${EOS_SDK_INCLUDE_DIR}")

if(DEV)
	include_directories("${PROJECT_SOURCE_DIR}/../Shared/NotForLicensees/Source/Core")
	add_definitions(-DALLOW_RESERVED_PLATFORM_OPTIONS)
endif(DEV)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

set(THIRD_PARTY_DEPENDENCY_LIBS "")

#Search for dependencies
if(BUILD_WITH_SDL)
	#SDL
	find_package (SDL2 REQUIRED)
	find_package (SDL2TTF REQUIRED)
	if (NOT SDL2_FOUND OR NOT SDL2TTF_FOUND)
		message (STATUS "SDL not found!")
	else ()
		include_directories(${SDL2_INCLUDE_DIR})
	endif()
	
	#OpenGL
	set(OpenGL_GL_PREFERENCE "GLVND")
	find_package(OpenGL)
	
	#GLEW 
	find_package(GLEW)
	if (NOT GLEW_FOUND)
		message (STATUS "GLEW not found!")
	else()
		#HACK to fix the issue with glew & cmake when glew-config.cmake is present for user locally in glew folder.
		#Once FindGlew.cmake gets an update we can get rid of this if block
		if((GLEW_FOUND AND TARGET GLEW::GLEW AND NOT DEFINED GLEW_INCLUDE_DIR) OR GLEW_LIBRARIES STREQUAL "GLEW_LIBRARY-NOTFOUND")
			get_target_property(GLEW_INCLUDE_DIRS GLEW::GLEW INTERFACE_INCLUDE_DIRECTORIES)
			set(GLEW_INCLUDE_DIR ${GLEW_INCLUDE_DIRS})
			get_target_property(_GLEW_LINK_INTERFACE GLEW::GLEW LOCATION_RELEASE)
			
			set(GLEW_LIBRARIES ${_GLEW_LINK_INTERFACE})
			unset(_GLEW_LINK_INTERFACE)
			
			message (STATUS "GLEW fix applied: GLEW_INCLUDE_DIRS: ${GLEW_INCLUDE_DIRS}")
			message (STATUS "GLEW_LIBRARIES: ${GLEW_LIBRARIES}")
		endif()
	
		include_directories(${GLEW_INCLUDE_DIR})
	endif()
	if(SDL2_FOUND AND SDL2TTF_FOUND AND OPENGL_FOUND AND GLEW_FOUND)
		set(THIRD_PARTY_DEPENDENCY_LIBS
			${GLEW_LIBRARIES}
			${OPENGL_glu_LIBRARY}
			${OPENGL_opengl_LIBRARY}
			${SDL2_LIBRARIES}
			${SDL2_LIBRARY}
			${SDL2TTF_LIBRARY}
		)
	message (STATUS "All 3rd parties found: ${THIRD_PARTY_DEPENDENCY_LIBS}")
	endif()
endif(BUILD_WITH_SDL)

if(BUILD_WITH_DXTK)
	message (STATUS "Not supported yet!")
endif(BUILD_WITH_DXTK)

set(SOURCES
	../Shared/Source/pch.cpp
	../Shared/Source/Core/AccountHelpers.cpp
	../Shared/Source/Core/Authentication.cpp
	../Shared/Source/Core/Friends.cpp
	../Shared/Source/Core/EosUI.cpp
	../Shared/Source/Core/Metrics.cpp
	../Shared/Source/Core/Platform.cpp
	../Shared/Source/Core/Player.cpp
	../Shared/Source/Core/Users.cpp
	
	../Shared/Source/Graphics/GUI/AssetUtils.cpp
	../Shared/Source/Graphics/GUI/AuthDialogs.cpp
	../Shared/Source/Graphics/GUI/ConsoleDialog.cpp
	../Shared/Source/Graphics/GUI/ExitDialog.cpp
	../Shared/Source/Graphics/GUI/PopupDialog.cpp
	../Shared/Source/Graphics/GUI/Button.cpp
	../Shared/Source/Graphics/GUI/Checkbox.cpp
	../Shared/Source/Graphics/GUI/ProgressBar.cpp
	../Shared/Source/Graphics/GUI/FriendsDialog.cpp
	../Shared/Source/Graphics/GUI/FriendInfo.cpp
	../Shared/Source/Graphics/GUI/FriendList.cpp

	../Shared/Source/Graphics/GUI/Scroller.cpp
	../Shared/Source/Graphics/GUI/Sprite.cpp
	../Shared/Source/Graphics/GUI/TextField.cpp
	../Shared/Source/Graphics/GUI/TextLabel.cpp

	../Shared/Source/Graphics/GUI/TextureManager.cpp
	../Shared/Source/Graphics/GUI/Texture.cpp
	../Shared/Source/Graphics/GUI/AnimatedTexture.cpp
	../Shared/Source/Graphics/GUI/TextView.cpp
	../Shared/Source/Graphics/GUI/Widget.cpp
	
	../Shared/Source/Graphics/GUI/SelectableStringViewListEntry.cpp
	../Shared/Source/Graphics/GUI/TableRowView.cpp

	../Shared/Source/Graphics/GUI/Dialog.cpp

	../Shared/Source/Graphics/GUI/Font.cpp
	../Shared/Source/Graphics/GUI/SDLTTF.cpp
	../Shared/Source/Graphics/Console.cpp
	../Shared/Source/Graphics/VectorRender.cpp
	../Shared/Source/Graphics/Model.cpp
	../Shared/Source/Graphics/SDLSpriteBatch.cpp
	
	../Shared/Source/Input/Input.cpp
	../Shared/Source/Input/SDLInput.cpp
	../Shared/Source/Main/Main.cpp
	../Shared/Source/Main/SDL/SDLMain.cpp
	../Shared/Source/Main/Windows/DeviceResources.cpp
	../Shared/Source/Main/Windows/WinMain.cpp
	../Shared/Source/Math/Vector2.cpp
	../Shared/Source/Math/Vector3.cpp
	../Shared/Source/Utils/CommandLine.cpp
	../Shared/Source/Utils/DebugLog.cpp
	../Shared/Source/Utils/StringUtils.cpp
	../Shared/Source/Utils/Utils.cpp
	../Shared/Source/Utils/Settings.cpp
	
	../Shared/Source/BaseGame.cpp
	../Shared/Source/BaseLevel.cpp
	../Shared/Source/BaseMenu.cpp
	
	Source/Game.cpp
	Source/Menu.cpp
	Source/Mods.cpp
	Source/ModsDialog.cpp
	Source/ModsTableRowView.cpp
	Source/ModsTableDialog.cpp
)
add_definitions(-DEOS_SAMPLE_MODS)

if(APPLE)
	# set how it shows up in the Info.plist file
	set(MACOSX_BUNDLE_ICON_FILE Game.icns) 
	# set where in the bundle to put the icns file
	set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Source/Game.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	# include the icns file in the target
	set(APPICONS ${CMAKE_CURRENT_SOURCE_DIR}/Source/Game.icns)

	# default asset path prefix based on executable location within .app bundle (Contents/MacOS)
	add_definitions(-DEOS_ASSETS_PATH_PREFIX=L"../../../")
endif(APPLE)

set(APP_NAME Mods)

add_executable(${APP_NAME} WIN32 MACOSX_BUNDLE ${SOURCES} ${APPICONS})

target_link_libraries(${APP_NAME} ${THIRD_PARTY_DEPENDENCY_LIBS} ${EOS_SDK_LIBRARIES})

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

# Get embedded FreeType framework folder within SDL2 TTF framework
list(GET SDL2TTF_LIBRARY 0 SDL2TTF_FREETYPE_LIBRARY)

# Directories to look in for dependencies
set(DIRS ${EOS_SDK_BIN_DIR}
		 ${SDL2_LIBRARY}
		 ${SDL2TTF_LIBRARY}
		 ${OPENGL_opengl_LIBRARY}
		 ${SDL2TTF_FREETYPE_LIBRARY}/Frameworks/FreeType.framework
	)

install(CODE "include(BundleUtilities)
    fixup_bundle(\"${APPS}\" \"\" \"${DIRS}\")")