cmake_minimum_required(VERSION 3.23...3.31)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(misc_target)
include(set_cxx_standard)
include(target_folder)
include(vs_startup_project)

set_cxx_standard(17)

set(TIMELINE_CONTROL_PROJECT_NAME "Timeline Control" CACHE "STRING" "Default project name for IDEs")
option(TIMELINE_CONTROL_WITH_WX "Build the wxWidgets timeline control" ON)
option(TIMELINE_CONTROL_WITH_IMGUI "Build the Dear ImGui timeline control" OFF)

if(TIMELINE_CONTROL_WITH_WX)
    list(APPEND VCPKG_MANIFEST_FEATURES "wx")
endif()
if(TIMELINE_CONTROL_WITH_IMGUI)
    list(APPEND VCPKG_MANIFEST_FEATURES "imgui")
endif()
list(REMOVE_DUPLICATES VCPKG_MANIFEST_FEATURES)

project("${TIMELINE_CONTROL_PROJECT_NAME}" CXX)

if(TIMELINE_CONTROL_WITH_IMGUI)
    find_package(imgui CONFIG REQUIRED)
endif()

include(CTest)

add_subdirectory(libs)
if(TIMELINE_CONTROL_WITH_WX)
    add_subdirectory(tools)
    vs_startup_project(timeline-viewer)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

misc_target(FILES
    "CMakePresets.json"
    "ReadMe.md")
misc_group_sources("CMake Scripts" FILES
    "cmake/misc_target.cmake"
    "cmake/set_cxx_standard.cmake"
    "cmake/target_folder.cmake"
    "cmake/vs_startup_project.cmake")
misc_group_sources("Editing" FILES
    ".clang-format"
    ".editorconfig")
misc_group_sources("Git" FILES
    ".gitignore"
    ".gitmodules")
misc_group_sources("GitHub" FILES
    ".github/workflows/cmake.yml")
misc_group_sources("VcPkg" FILES
    "vcpkg.json")
