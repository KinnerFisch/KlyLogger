cmake_minimum_required(VERSION 3.20)
project(KlyLogger VERSION 1.0.0 LANGUAGES CXX)

# Static archives are always produced with an optimized configuration.
# Restrict multi-config generators and force single-config generators to
# Release so a Debug invocation cannot silently produce an unoptimized archive.
if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Available build configurations" FORCE)
else()
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# CMake's MSVC Release defaults add /O2 /Ob2. Remove those lower settings so
# the explicit /Ox /Ob3 options below do not merely override them (and do not
# emit D9025 override warnings).
if(MSVC)
	string(REGEX REPLACE "(^| )[/-]O2( |$)" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	string(REGEX REPLACE "(^| )[/-]Ob2( |$)" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Release compiler flags" FORCE)
endif()

# Require interprocedural optimization: /GL + /LTCG on MSVC and LTO on
# GCC/Clang. Configuration fails instead of producing a non-LTO archive when
# the selected compiler/toolchain cannot support it.
include(CheckIPOSupported)
check_ipo_supported(RESULT KLY_LOGGER_IPO_SUPPORTED OUTPUT KLY_LOGGER_IPO_ERROR LANGUAGES CXX)
if(NOT KLY_LOGGER_IPO_SUPPORTED)
	message(FATAL_ERROR "KlyLogger requires whole-program/LTO optimization: ${KLY_LOGGER_IPO_ERROR}")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include/fmt/format.h")
	message(FATAL_ERROR
		"fmt submodule is missing. Run: git submodule update --init --recursive")
endif()

# Build every behavior/runtime combination so applications can select one when
# linking. STATIC is intentional: no target creates a DLL/shared object.
function(kly_logger_add_variant target_name output_name no_log_file no_handle_cache msvc_runtime)
	# Compile fmt directly into every KlyLogger archive. Consumers link one
	# KlyLogger .lib/.a and do not need a separate fmt binary.
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

	if(MSVC)
		# A static C++ archive cannot be runtime-library agnostic: KlyLogger's
		# public API crosses the library boundary with STL types. Build explicit
		# /MD and /MT archives so consumers can select the matching CRT.
		set_property(TARGET ${target_name} PROPERTY MSVC_RUNTIME_LIBRARY ${msvc_runtime})
		# fmt's Unicode support requires UTF-8 source/execution encoding in every
		# C++ translation unit that includes KlyLogger.hpp.
		target_compile_options(${target_name} PUBLIC /utf-8)
		# /Ox: maximum optimization; /GL: whole-program optimization;
		# /Ob3, /Oi, /Ot: aggressive inlining, intrinsics, and favor speed;
		# /Gy, /Gw, /Zc:inline: function/global-data COMDAT optimization.
		target_compile_options(${target_name} PRIVATE
			/W4 /permissive-
			/Ox /Ob3 /Oi /Ot /Oy /GL /Gy /Gw /Zc:inline)
		set_property(TARGET ${target_name} APPEND PROPERTY STATIC_LIBRARY_OPTIONS /LTCG)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
		# -O3 plus IPO/LTO provides maximum cross-translation-unit optimization;
		# the remaining flags favor speed and permit dead-section elimination by
		# the consumer's final linker.
		target_compile_options(${target_name} PRIVATE
			-Wall -Wextra -Wpedantic
			-O3 -finline-functions -funroll-loops
			-ffunction-sections -fdata-sections)
	else()
		message(FATAL_ERROR "Unsupported compiler for required /Ox or -O3 optimization: ${CMAKE_CXX_COMPILER_ID}")
	endif()
endfunction()

if(MSVC)
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
	kly_logger_add_variant(KlyLoggerNoOutputHandleCache KlyLoggerNoOutputHandleCache FALSE TRUE "")
	kly_logger_add_variant(KlyLoggerNoLogFileNoOutputHandleCache KlyLoggerNoLogFileNoOutputHandleCache TRUE TRUE "")

	set_target_properties(KlyLogger PROPERTIES EXPORT_NAME KlyLogger)
	set_target_properties(KlyLoggerNoLogFile PROPERTIES EXPORT_NAME NoLogFile)
	set_target_properties(KlyLoggerNoOutputHandleCache PROPERTIES EXPORT_NAME NoOutputHandleCache)
	set_target_properties(KlyLoggerNoLogFileNoOutputHandleCache PROPERTIES EXPORT_NAME NoLogFileNoOutputHandleCache)

	add_library(KlyLogger::KlyLogger ALIAS KlyLogger)
	add_library(KlyLogger::NoLogFile ALIAS KlyLoggerNoLogFile)
	add_library(KlyLogger::NoOutputHandleCache ALIAS KlyLoggerNoOutputHandleCache)
	add_library(KlyLogger::NoLogFileNoOutputHandleCache ALIAS KlyLoggerNoLogFileNoOutputHandleCache)

	set(KLY_LOGGER_TARGETS
		KlyLogger
		KlyLoggerNoLogFile
		KlyLoggerNoOutputHandleCache
		KlyLoggerNoLogFileNoOutputHandleCache)
endif()

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
