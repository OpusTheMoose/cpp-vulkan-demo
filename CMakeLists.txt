cmake_minimum_required(VERSION 4.4.2)
set(CMAKE_CXX_COMPILER clang++)
project(game CXX)
include(FetchContent)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

FetchContent_Declare(SDL3 
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.20/SDL3-3.2.20.tar.gz
        
)
FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)

FetchContent_Declare(
        imgui 
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
)
FetchContent_MakeAvailable(imgui)

find_program(SLANGC_EXECUTABLE slangc REQUIRED)

function(add_slang_shader_target TARGET)
  cmake_parse_arguments(SHADER "" "" "SOURCES" ${ARGN})

  set(SHADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/src/shaders")
  set(ENTRY_POINTS
      -entry vertMain
      -entry fragMain
  )
  set(SPV_OUTPUTS)

  foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_SOURCE}" NAME_WE)
    set(SPV_OUTPUT "${SHADERS_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
      OUTPUT "${SPV_OUTPUT}"
      COMMAND "${SLANGC_EXECUTABLE}"
              "${SHADER_SOURCE}"
              -target spirv
              -profile spirv_1_4
              -emit-spirv-directly
              -fvk-use-entrypoint-name
              ${ENTRY_POINTS}
              -o "${SPV_OUTPUT}"
      DEPENDS "${SHADER_SOURCE}"
      WORKING_DIRECTORY "${SHADERS_DIR}"
      COMMENT "Compiling ${SHADER_NAME}.slang"
      VERBATIM
    )

    list(APPEND SPV_OUTPUTS "${SPV_OUTPUT}")
  endforeach()

  add_custom_target("${TARGET}" DEPENDS ${SPV_OUTPUTS})
endfunction()

add_slang_shader_target(
  slang_shaders
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/src/shaders/instance.slang"
    "${CMAKE_CURRENT_LIST_DIR}/src/shaders/line.slang"
    "${CMAKE_CURRENT_LIST_DIR}/src/shaders/grid_floor.slang"

)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp

    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

add_executable(game main.cpp ) 


target_sources(game PUBLIC FILE_SET CXX_MODULES FILES  src/utils/types.cpp  src/window/window.cpp src/utils/logger.cpp src/window/event.cpp src/application/app.cpp
src/renderer/vulkan_renderer.cpp src/utils/file.cpp src/objects/vertex.cpp src/objects/mesh.cpp src/utils/camera.cpp  src/objects/draw3d.cpp src/simulation/simulation.cpp 
src/application/raycast.cpp )
target_compile_options(game PRIVATE -Wall -Wextra -Wpedantic -Werror  -g -O3 )
target_compile_definitions(game PRIVATE TRACY_ENABLE)
        

target_link_libraries(imgui PUBLIC SDL3::SDL3 Vulkan::Vulkan)

target_link_libraries(game PUBLIC SDL3::SDL3 Vulkan::Vulkan TracyClient imgui)
add_dependencies(game slang_shaders)
include(CTest)
enable_testing()

