# Copyright (c) 2026 Richard Thomson

# Qt finds platform plugins beside the executable; vcpkg deploys Qt DLLs.
function(deploy_qt_plugins target)
    foreach(plugin IN LISTS ARGN)
        if(NOT TARGET Qt6::${plugin})
            message(FATAL_ERROR "Required Qt platform plugin is missing: ${plugin}")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::${plugin}>" "$<TARGET_FILE_DIR:${target}>/platforms"
            VERBATIM)
    endforeach()
endfunction()
