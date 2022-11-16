# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "video_player_win")
project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "video_player_win_plugin")

######## Jacky {
# download nuget.exe (Microsoft) and Microsoft WebView2 SDK
# ref: https://github.com/jnschulze/flutter-webview-windows/blob/main/windows/CMakeLists.txt

set(NUGET_URL https://dist.nuget.org/win-x86-commandline/v5.10.0/nuget.exe)
set(NUGET_SHA256 852b71cc8c8c2d40d09ea49d321ff56fd2397b9d6ea9f96e532530307bbbafd3)

set(WIL_VERSION "1.0.220914.1")

find_program(NUGET nuget)
if(NOT NUGET)
  set(NUGET ${CMAKE_BINARY_DIR}/nuget.exe)

  if (NOT EXISTS ${NUGET})
    message(NOTICE "Nuget is not installed.\nStart downloading nuget. Please wait...")
    file(DOWNLOAD ${NUGET_URL} ${NUGET})
  endif()

  file(SHA256 ${NUGET} NUGET_DL_HASH)
  if (NOT NUGET_DL_HASH STREQUAL NUGET_SHA256)
    message(FATAL_ERROR "Integrity check for ${NUGET} failed.")
  endif()
endif()

add_custom_target(${PROJECT_NAME}_DEPENDENCIES_DOWNLOAD ALL)
add_custom_command(
  TARGET ${PROJECT_NAME}_DEPENDENCIES_DOWNLOAD PRE_BUILD
  COMMAND ${NUGET} install Microsoft.Windows.ImplementationLibrary -Version ${WIL_VERSION} -ExcludeVersion -OutputDirectory ${CMAKE_BINARY_DIR}/packages
  DEPENDS ${NUGET}
)
######## Jacky }

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
  "video_player_win_plugin.cpp"
  "video_player_win_plugin.h"
)

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  "include/video_player_win/video_player_win_plugin_c_api.h"
  "video_player_win_plugin_c_api.cpp"
  ${PLUGIN_SOURCES}
  "my_grabber_player.cpp" #Jacky
)

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})

# Jacky {
target_link_libraries(${PLUGIN_NAME} PRIVATE ${CMAKE_BINARY_DIR}/packages/Microsoft.Web.WebView2/build/native/Microsoft.Web.WebView2.targets)
target_link_libraries(${PLUGIN_NAME} PRIVATE ${CMAKE_BINARY_DIR}/packages/Microsoft.Windows.ImplementationLibrary/build/native/Microsoft.Windows.ImplementationLibrary.targets)
# Jacky }


# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# Source include directories and library dependencies. Add any plugin-specific
# dependencies here.
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin)

# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(video_player_win_bundled_libraries
  ""
  PARENT_SCOPE
)
