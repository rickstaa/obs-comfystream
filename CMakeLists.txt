cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
find_package(LibDataChannel REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs LibDataChannel::LibDataChannel nlohmann_json::nlohmann_json)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.cpp src/comfystream_client.cpp)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Set Ubuntu-specific install paths
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "/etc/os-release")
  file(READ "/etc/os-release" OS_RELEASE_CONTENT)
  if(OS_RELEASE_CONTENT MATCHES "Ubuntu")
    set(LIB_INSTALL_DIR "/usr/lib/x86_64-linux-gnu/obs-plugins")
    set(SHARE_INSTALL_DIR "/usr/share/obs/obs-plugins/obs-comfystream")
  endif()
endif()

# Install plugin and data files
install(TARGETS ${CMAKE_PROJECT_NAME}
        LIBRARY DESTINATION "${LIB_INSTALL_DIR}")
install(DIRECTORY data/ DESTINATION "${SHARE_INSTALL_DIR}/data")
