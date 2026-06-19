# CMakeLists.txt - Packaging ready (ZIP/7Z), CLion/MinGW/MSVC friendly

cmake_minimum_required(VERSION 3.21)
project(ppgso CXX)

#
# CONFIGURATION
#
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Install do build/_install (nech nešpiníš source tree)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/_install" CACHE PATH "" FORCE)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Working dir pre VS/CLion (aby relatívne cesty fungovali pri spustení z IDE)
set(VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Convert .glsl -> .cpp resources (ppgso shader pack)
include(add_resources)

#
# DEPENDENCIES
#

# Windows bundled deps (dependencies/...)
if (MINGW)
  get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)

  message(STATUS "MinGW bin dir: ${MINGW_BIN}")

  install(FILES
          "${MINGW_BIN}/libstdc++-6.dll"
          "${MINGW_BIN}/libgcc_s_seh-1.dll"
          "${MINGW_BIN}/libwinpthread-1.dll"
          DESTINATION .
  )
  list(APPEND CMAKE_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/dependencies/include/")
  list(APPEND CMAKE_LIBRARY_PATH "${CMAKE_SOURCE_DIR}/dependencies/lib/mingw")
elseif (MSVC)
  list(APPEND CMAKE_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/dependencies/include/")

  if(MSVC_VERSION EQUAL 1500)
    list(APPEND CMAKE_LIBRARY_PATH "${CMAKE_SOURCE_DIR}/dependencies/lib/vc2015")
  elseif(MSVC_VERSION GREATER_EQUAL 1900)
    list(APPEND CMAKE_LIBRARY_PATH "${CMAKE_SOURCE_DIR}/dependencies/lib/vc2019")
  endif()

  set(USE_STRICT_COMPILE_WARNINGS OFF CACHE BOOL "" FORCE)
endif ()

# Warnings for Debug mode
option(USE_STRICT_COMPILE_WARNINGS "Use strict compilation warnings in debug mode." ON)
set(STRICT_COMPILE_FLAGS "-Wpedantic -Wall -Wno-c++98-compat -Wextra -Wno-sign-conversion -Wno-unused-parameter")
if (USE_STRICT_COMPILE_WARNINGS)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${STRICT_COMPILE_FLAGS}")
endif ()

# Required packages
find_package(GLFW3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLM REQUIRED)
find_package(OpenGL REQUIRED)

# Optional: ASSIMP
find_package(ASSIMP)
if (ASSIMP_FOUND)
  add_definitions(-DUSE_ASSIMP=1)
endif()

# Optional: OpenMP
find_package(OpenMP)
if(OPENMP_FOUND)
  list(APPEND CMAKE_CXX_FLAGS ${OpenMP_CXX_FLAGS})
endif()

#
# Shaders / Materials (ppgso internal shader pack)
#
set(PPGSO_SHADER_SRC
        shader/color_vert.glsl shader/color_frag.glsl
        shader/convolution_vert.glsl shader/convolution_frag.glsl
        shader/diffuse_vert.glsl shader/diffuse_frag.glsl
        shader/texture_vert.glsl shader/texture_frag.glsl
)
add_resources(shaders ${PPGSO_SHADER_SRC})

#
# PPGSO library
#
if (ASSIMP_FOUND)
  message(STATUS "Using ASSIMP object loader")
  add_library(ppgso STATIC
          ppgso/Mesh_Assimp.cpp
          ppgso/tiny_obj_loader.cpp
          ppgso/shader.cpp
          ppgso/image.cpp
          ppgso/image_bmp.cpp
          ppgso/image_raw.cpp
          ppgso/texture.cpp
          ppgso/window.cpp
  )
else ()
  message(STATUS "Using TINY object loader")
  add_library(ppgso STATIC
          ppgso/Mesh_Tiny.cpp
          ppgso/tiny_obj_loader.cpp
          ppgso/shader.cpp
          ppgso/image.cpp
          ppgso/image_bmp.cpp
          ppgso/image_raw.cpp
          ppgso/texture.cpp
          ppgso/window.cpp
  )
endif ()

# GLM radians + GLEW static
target_compile_definitions(ppgso PUBLIC -DGLM_FORCE_RADIANS -DGLEW_STATIC)

# Link deps
if (ASSIMP_FOUND)
  target_link_libraries(ppgso PUBLIC
          ${GLFW_LIBRARIES}
          ${GLEW_LIBRARIES}
          ${OPENGL_LIBRARIES}
          ${ASSIMP_LIBRARIES}
  )
else ()
  target_link_libraries(ppgso PUBLIC
          ${GLFW_LIBRARIES}
          ${GLEW_LIBRARIES}
          ${OPENGL_LIBRARIES}
  )
endif ()

target_include_directories(ppgso PUBLIC
        ppgso
        ${GLFW_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS}
        ${GLM_INCLUDE_DIRS}
        ${OPENGL_INCLUDE_DIRS}
        ${ASSIMP_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

#
# TARGET: gl9_scene
#
add_executable(gl9_scene
        src/gl9_scene/main.cpp
        src/gl9_scene/camera.cpp
        src/gl9_scene/t_object.cpp
        src/gl9_scene/PostProcess.cpp
        src/gl9_scene/particleSystem.cpp

        # headers (optional in list, but ok for IDE)
        src/gl9_scene/t_object.h
        src/gl9_scene/camera.h
        src/gl9_scene/particleShaders.h
        src/gl9_scene/particleSystem.h
        src/gl9_scene/PostProcess.h
        src/gl9_scene/rigid.h

        # shadery, ktoré načítavaš z disk-u
        src/gl9_scene/bloom_shader.fs
        src/gl9_scene/bloom_shader.vs
        src/gl9_scene/blur_shader.fs
        src/gl9_scene/blur_shader.vs
        src/gl9_scene/skybox.fs
        src/gl9_scene/skybox.vs
)

target_link_libraries(gl9_scene PRIVATE ppgso shaders)

# Kopírovanie assets do build dir (aby to bežalo v CLion po "Run")
add_custom_command(TARGET gl9_scene POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/objects"
        "$<TARGET_FILE_DIR:gl9_scene>/objects"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/textures"
        "$<TARGET_FILE_DIR:gl9_scene>/textures"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/bloom_shader.fs"
        "$<TARGET_FILE_DIR:gl9_scene>/bloom_shader.fs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/bloom_shader.vs"
        "$<TARGET_FILE_DIR:gl9_scene>/bloom_shader.vs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/blur_shader.fs"
        "$<TARGET_FILE_DIR:gl9_scene>/blur_shader.fs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/blur_shader.vs"
        "$<TARGET_FILE_DIR:gl9_scene>/blur_shader.vs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/skybox.fs"
        "$<TARGET_FILE_DIR:gl9_scene>/skybox.fs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/skybox.vs"
        "$<TARGET_FILE_DIR:gl9_scene>/skybox.vs"
)

#
# INSTALLATION (čo ide do ZIP/7Z)
#

# exe do rootu balíka
install(TARGETS gl9_scene RUNTIME DESTINATION .)

# assets (objects, textures) do rootu balíka
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/gl9_scene/objects"  DESTINATION .)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/src/gl9_scene/textures" DESTINATION .)

# shadery vedľa exe
install(FILES
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/bloom_shader.fs"
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/bloom_shader.vs"
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/blur_shader.fs"
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/blur_shader.vs"
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/skybox.fs"
        "${CMAKE_SOURCE_DIR}/src/gl9_scene/skybox.vs"
        DESTINATION .
)

# (voliteľné) licencie dependency (ak chceš pribaľovať)
# install(FILES
#   "${CMAKE_SOURCE_DIR}/dependencies/copying_glew.txt"
#   "${CMAKE_SOURCE_DIR}/dependencies/copying_glfw.txt"
#   "${CMAKE_SOURCE_DIR}/dependencies/copying_glm.txt"
#   "${CMAKE_SOURCE_DIR}/dependencies/LICENSE_assimp.txt"
#   DESTINATION .
# )


#
# PACKAGING (ZIP / 7Z)
#
set(CPACK_PACKAGE_NAME "gl9_scene")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_GENERATOR "ZIP;7Z")
include(CPack)
