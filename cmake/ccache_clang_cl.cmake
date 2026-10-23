# clang-cl with ccache (CI): CMake asks clang-cl for Ninja's dependency files through -clang:-MD/-MF/-MT, and ccache
# refuses to cache anything with a /clang: option. MSVC-style /showIncludes gives Ninja the same information and ccache
# supports it. Included by CMakeLists.txt, and by build_windows.sh's N64Recomp tools build via CMAKE_PROJECT_INCLUDE.
if (CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" AND CMAKE_GENERATOR MATCHES "Ninja"
    AND CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache")
    foreach (lang C CXX)
        set(CMAKE_DEPFILE_FLAGS_${lang} "/showIncludes")
        set(CMAKE_${lang}_DEPFILE_FORMAT msvc)
    endforeach()
endif()
