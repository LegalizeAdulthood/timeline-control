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
option(TIMELINE_CONTROL_WITH_QT "Build the Qt timeline control" OFF)

if(TIMELINE_CONTROL_WITH_WX)
    list(APPEND VCPKG_MANIFEST_FEATURES "wx")
endif()
if(TIMELINE_CONTROL_WITH_IMGUI)
    list(APPEND VCPKG_MANIFEST_FEATURES "imgui")
endif()
if(TIMELINE_CONTROL_WITH_QT)
    list(APPEND VCPKG_MANIFEST_FEATURES "qt")
endif()
list(REMOVE_DUPLICATES VCPKG_MANIFEST_FEATURES)

project("${TIMELINE_CONTROL_PROJECT_NAME}" CXX)

if(TIMELINE_CONTROL_WITH_IMGUI)
    find_package(imgui CONFIG REQUIRED)
endif()
if(TIMELINE_CONTROL_WITH_QT)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Widgets)
    include(deploy_qt_plugins)
endif()

include(CTest)

add_subdirectory(libs)
add_subdirectory(tools)
if(TIMELINE_CONTROL_WITH_WX)
    vs_startup_project(timeline-viewer)
elseif(TIMELINE_CONTROL_WITH_IMGUI)
    vs_startup_project(imgui-timeline-viewer)
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
    "cmake/vs_startup_project.cmake"
    "cmake/deploy_qt_plugins.cmake")
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
