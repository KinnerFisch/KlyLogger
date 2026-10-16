cmake_minimum_required(VERSION 3.20)
project(KlyLogger VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Toolchain detection
#
# MSVC          : Microsoft cl.exe
# clang-cl      : Clang with the MSVC-compatible driver / ABI (CMAKE_CXX_SIMULATE_ID == MSVC)
# GCC / Clang   : native Linux toolchains
#
# On Windows only MSVC and clang-cl are supported.
# GCC/MinGW is rejected because KlyLogger crosses the library boundary with STL types and MinGW uses a different C++ ABI (libstdc++) that is not compatible with MSVC consumers.
# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
set(KLY_LOGGER_IS_MSVC FALSE)
set(KLY_LOGGER_IS_CLANG_CL FALSE)

# clang-cl also defines MSVC, so the clang-cl signature has to be tested first.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(KLY_LOGGER_IS_CLANG_CL TRUE)
elseif(MSVC)
    set(KLY_LOGGER_IS_MSVC TRUE)
endif()

if(WIN32 AND NOT KLY_LOGGER_IS_MSVC AND NOT KLY_LOGGER_IS_CLANG_CL)
    message(FATAL_ERROR
        "KlyLogger on Windows supports only MSVC (cl.exe) or clang-cl. "
        "GCC/MinGW is not supported because it uses a different C++ ABI.")
endif()

if(NOT WIN32 AND NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    message(FATAL_ERROR
        "KlyLogger on Linux supports only GCC or Clang. "
        "Detected compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Build configuration
#
# Static archives are always produced with an optimized configuration.
# Multi-config generators are restricted to Release; single-config generators are forced to Release so a Debug invocation cannot silently produce an unoptimized archive.
# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Available build configurations" FORCE)
else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Interprocedural optimization (IPO/LTO)
#
# - MSVC      : /GL + /LTCG
# - clang-cl  : -flto=full (LLVM bitcode LTO, linked by lld-link or the clang driver)
# - Clang     : -flto=full
# - GCC       : -flto
#
# All three producers must be paired with a compatible linker, and LTO objects from different toolchains must never be mixed.
# This is enforced by keeping the LTO flavor tied to the compiler that produced the archive.
# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
include(CheckIPOSupported)
check_ipo_supported(RESULT KLY_LOGGER_IPO_SUPPORTED OUTPUT KLY_LOGGER_IPO_ERROR LANGUAGES CXX)
if(NOT KLY_LOGGER_IPO_SUPPORTED)
    message(FATAL_ERROR "KlyLogger requires whole-program/LTO optimization: ${KLY_LOGGER_IPO_ERROR}")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include/fmt/format.h")
    message(FATAL_ERROR "fmt submodule is missing. Run: git submodule update --init --recursive")
endif()

# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Remove CMake's default MSVC Release optimization flags so the explicit optimization options below do not merely override them and do not emit D9025 override warnings.
# clang-cl inherits the same CMAKE_CXX_FLAGS_RELEASE.
# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
if(KLY_LOGGER_IS_MSVC OR KLY_LOGGER_IS_CLANG_CL)
    foreach(flag_var CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE)
        string(REGEX REPLACE "(^| )[/-]O2( |$)" " " ${flag_var} "${${flag_var}}")
        string(REGEX REPLACE "(^| )[/-]Ob2( |$)" " " ${flag_var} "${${flag_var}}")
        set(${flag_var} "${${flag_var}}" CACHE STRING "Release compiler flags" FORCE)
    endforeach()
endif()

# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Build every behavior/runtime combination so applications can select one when linking.
#
#   no_log_file    : compile out the file-logging backend
#   no_handle_cache: compile out the Windows stderr handle cache
#   msvc_runtime   : MultiThreadedDLL (/MD) or MultiThreaded (/MT)
# ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
function(kly_logger_add_variant target_name output_name no_log_file no_handle_cache msvc_runtime)
    # Compile fmt directly into every KlyLogger archive.
    # Consumers link one KlyLogger .lib/.a and do not need a separate fmt binary.
    add_library(${target_name} STATIC KlyLogger.cpp fmt/src/format.cc)
    set_target_properties(${target_name} PROPERTIES
        OUTPUT_NAME ${output_name}
        INTERPROCEDURAL_OPTIMIZATION TRUE)
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    target_include_directories(${target_name} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fmt/include>
        $<INSTALL_INTERFACE:include>)

    if(no_log_file)
        target_compile_definitions(${target_name} PRIVATE KLY_LOGGER_OPTION_NO_LOG_FILE)
    endif()
    if(no_handle_cache)
        target_compile_definitions(${target_name} PRIVATE KLY_LOGGER_OPTION_NO_CACHE_FOR_OUTPUT_HANDLE)
    endif()

    # ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    # Maximum cross-translation-unit optimization for every toolchain.
    #
    # Note on -march:
    #   Static archives are distributed across machines, so -march=native or a fixed -march would risk illegal-instruction crashes on older CPUs.
    #   Only -mtune=generic is applied; it tunes scheduling without raising the ISA baseline.
    #   Enable KLY_LOGGER_NATIVE_ARCH below if you truly need it.
    # ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    option(KLY_LOGGER_NATIVE_ARCH
        "Tune for the build machine's CPU (-march=native / /arch:AVX2). Off for portable archives."
        OFF)

    if(KLY_LOGGER_IS_MSVC)
        # -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        # MSVC (cl.exe)
        #
        # /GL produces LTCG intermediate objects that only MSVC's link.exe can consume, so MSVC-built archives must be linked with MSVC's link.exe.
        # -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY ${msvc_runtime})
        # fmt's Unicode support requires UTF-8 source/execution encoding in every C++ translation unit that includes KlyLogger.hpp.
        target_compile_options(${target_name} PUBLIC /utf-8)
        target_compile_options(${target_name} PRIVATE
            /W4 /permissive-
            /O2 /Ob3 /GL /Gy /Gw /Zc:inline)

        if(KLY_LOGGER_NATIVE_ARCH)
            target_compile_options(${target_name} PRIVATE /arch:AVX2)
        endif()

        set_property(TARGET ${target_name} APPEND PROPERTY STATIC_LIBRARY_OPTIONS /LTCG)

    elseif(KLY_LOGGER_IS_CLANG_CL)
        # -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        # clang-cl (Windows, MSVC-compatible driver, LLVM backend)
        #
        # clang-cl accepts both MSVC-style (/O2, /W4) and LLVM-style (-O3, -flto) options.
        # LLVM-style options are used here because they reach clang's own optimizer directly without going through an /O2 mapping,
        # which yields the same code as the reference clang release build.
        #
        # Uses the Microsoft STL and MSVC ABI, so it is binary compatible with MSVC consumers when LTO is not involved.
        # clang-cl emits LLVM LTO objects with -flto, which lld-link (or the clang driver) consumes.
        #
        # /GL and /LTCG are MSVC-only and must not be used here.
        # -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY ${msvc_runtime})
        target_compile_options(${target_name} PUBLIC /utf-8)
        target_compile_options(${target_name} PRIVATE
            # --- diagnostics ---
            /W4
            # --- code generation (LLVM-style) ---
            /clang:-O3
            /clang:-flto=full
            /clang:-finline-functions
            /clang:-funroll-loops
            /clang:-fno-strict-aliasing
            # --- section-level dead code elimination ---
            /clang:-ffunction-sections
            /clang:-fdata-sections
            -Gy
            -Gw
            -Zc:inline
            # --- scheduling without raising the ISA baseline ---
            /clang:-mtune=generic)

        if(KLY_LOGGER_NATIVE_ARCH)
            target_compile_options(${target_name} PRIVATE -march=native)
        endif()

        # LTO objects from clang-cl must be linked by lld-link / clang driver.
        # -flto=full already implies the matching link-time codegen.

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        # -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        # GCC / Clang (Linux)
        #
        # -O3 plus full LTO provides maximum cross-translation-unit optimization;
        # the remaining flags favor speed and permit dead-section elimination by the consumer's final linker.
        # -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(KLY_LOGGER_LTO_OPTION -flto)
        else()
            set(KLY_LOGGER_LTO_OPTION -flto=full)
        endif()

        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic
            -O3 -finline-functions -funroll-loops
            ${KLY_LOGGER_LTO_OPTION}
            -fno-strict-aliasing
            -ffunction-sections -fdata-sections
            -mtune=generic)

        if(KLY_LOGGER_NATIVE_ARCH)
            target_compile_options(${target_name} PRIVATE -march=native)
        endif()

    else()
        message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()

# ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Instantiate the variants
# ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
if(KLY_LOGGER_IS_MSVC OR KLY_LOGGER_IS_CLANG_CL)
    set(KLY_LOGGER_TARGETS)
    foreach(runtime IN ITEMS MD MT)
        if(runtime STREQUAL MD)
            set(runtime_library MultiThreadedDLL)
        else()
            set(runtime_library MultiThreaded)
        endif()

        kly_logger_add_variant(KlyLogger_${runtime} KlyLogger_${runtime} FALSE FALSE ${runtime_library})
        kly_logger_add_variant(KlyLoggerNoLogFile_${runtime} KlyLoggerNoLogFile_${runtime} TRUE FALSE ${runtime_library})
        kly_logger_add_variant(KlyLoggerNoOutputHandleCache_${runtime} KlyLoggerNoOutputHandleCache_${runtime} FALSE TRUE ${runtime_library})
        kly_logger_add_variant(KlyLoggerNoLogFileNoOutputHandleCache_${runtime} KlyLoggerNoLogFileNoOutputHandleCache_${runtime} TRUE TRUE ${runtime_library})

        set_target_properties(KlyLogger_${runtime} PROPERTIES EXPORT_NAME KlyLogger_${runtime})
        set_target_properties(KlyLoggerNoLogFile_${runtime} PROPERTIES EXPORT_NAME NoLogFile_${runtime})
        set_target_properties(KlyLoggerNoOutputHandleCache_${runtime} PROPERTIES EXPORT_NAME NoOutputHandleCache_${runtime})
        set_target_properties(KlyLoggerNoLogFileNoOutputHandleCache_${runtime} PROPERTIES EXPORT_NAME NoLogFileNoOutputHandleCache_${runtime})

        add_library(KlyLogger::KlyLogger_${runtime} ALIAS KlyLogger_${runtime})
        add_library(KlyLogger::NoLogFile_${runtime} ALIAS KlyLoggerNoLogFile_${runtime})
        add_library(KlyLogger::NoOutputHandleCache_${runtime} ALIAS KlyLoggerNoOutputHandleCache_${runtime})
        add_library(KlyLogger::NoLogFileNoOutputHandleCache_${runtime} ALIAS KlyLoggerNoLogFileNoOutputHandleCache_${runtime})

        list(APPEND KLY_LOGGER_TARGETS
            KlyLogger_${runtime}
            KlyLoggerNoLogFile_${runtime}
            KlyLoggerNoOutputHandleCache_${runtime}
            KlyLoggerNoLogFileNoOutputHandleCache_${runtime})
    endforeach()
else()
    kly_logger_add_variant(KlyLogger KlyLogger FALSE FALSE "")
    kly_logger_add_variant(KlyLoggerNoLogFile KlyLoggerNoLogFile TRUE FALSE "")

    set_target_properties(KlyLogger PROPERTIES EXPORT_NAME KlyLogger)
    set_target_properties(KlyLoggerNoLogFile PROPERTIES EXPORT_NAME NoLogFile)

    add_library(KlyLogger::KlyLogger ALIAS KlyLogger)
    add_library(KlyLogger::NoLogFile ALIAS KlyLoggerNoLogFile)

    set(KLY_LOGGER_TARGETS
        KlyLogger
        KlyLoggerNoLogFile)
endif()

# ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
# Install rules
# ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS ${KLY_LOGGER_TARGETS}
    EXPORT KlyLoggerTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES KlyLogger.hpp KlyLogger.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY fmt/include/fmt DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/KlyLogger RENAME KlyLogger-LICENSE)
install(FILES fmt/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/KlyLogger RENAME fmt-LICENSE)
install(FILES THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/KlyLogger)
install(EXPORT KlyLoggerTargets NAMESPACE KlyLogger:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/KlyLogger)
