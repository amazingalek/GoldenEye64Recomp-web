# The changes lib/rt64, lib/RecompFrontend and lib/N64ModernRuntime need, which are upstream's commits: first this
# game's own, what podulator/GoldenEye64Recomp changes in its copies of them, as a patch a project, then
# N64Recomp-browser's patches/ (the web support and the recompiler changes this game needs) on top of them.
#
# - rt64.patch: a rotation that jumps between two frames is snapped rather than interpolated (rt64_rigid_body.cpp), and
#   ubershaders only, on every build but a page's, which builds the game's shaders before it draws with them.
# - RecompFrontend.patch: the Reload and Crouch inputs (src/game/input.cpp binds them, src/game/mouse.cpp reads them),
#   and RT64's postBlendNoiseNegative on, as GoldenEye's own render context always had it.
# - N64ModernRuntime.patch: ultramodern::debug_logging, which --debug-log turns on, and debug_printf printing behind it;
#   and the game id that select_rom, is_rom_valid and load_stored_rom take as const, as upstream's later runtime has it
#   (src/main/launcher.cpp passes its game entry's).
#
# CMakeLists.txt applies them when it configures, and the build scripts before they build N64Recomp from
# lib/N64ModernRuntime/N64Recomp, from the repository's root:
#
#   cmake -P lib-patches/apply.cmake lib
#
# A patch that is in already is left as it is, so running it again does nothing. git apply --reverse
# lib-patches/<name>.patch in a checkout takes one of this game's out (after N64Recomp-browser's, which
# lib/N64Recomp-browser/patches/apply.cmake's header says how to take out).

set(GOLDENEYE_LIB_PATCHES_DIR "${CMAKE_CURRENT_LIST_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/../lib/N64Recomp-browser/patches/apply.cmake")

function(goldeneye_apply_lib_patches lib)
    foreach (name rt64 RecompFrontend N64ModernRuntime)
        set(patch "${GOLDENEYE_LIB_PATCHES_DIR}/${name}.patch")
        set(dir "${lib}/${name}")
        execute_process(COMMAND git apply --reverse --check "${patch}" WORKING_DIRECTORY "${dir}"
            RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
        if (result EQUAL 0)
            continue()
        endif()
        execute_process(COMMAND git apply --whitespace=nowarn "${patch}" WORKING_DIRECTORY "${dir}"
            RESULT_VARIABLE result ERROR_VARIABLE error)
        if (NOT result EQUAL 0)
            message(FATAL_ERROR "lib-patches/${name}.patch doesn't apply to ${dir}:\n${error}")
        endif()
        message(STATUS "Patched ${name} (${dir}) with lib-patches/${name}.patch")
    endforeach()
    n64recomp_browser_apply_patches("${lib}")
endfunction()

if (CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    if (NOT CMAKE_ARGV3)
        message(FATAL_ERROR "usage: cmake -P lib-patches/apply.cmake <the folder holding rt64, N64ModernRuntime and RecompFrontend>")
    endif()
    get_filename_component(lib "${CMAKE_ARGV3}" ABSOLUTE)
    goldeneye_apply_lib_patches("${lib}")
endif()
