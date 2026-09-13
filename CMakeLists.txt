cmake_minimum_required(VERSION 3.16)
project(one44 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(clap INTERFACE)
target_include_directories(clap INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/clap/include)

set(ONE44_CORE_SOURCES
	src/core/constraint.cpp
	src/core/pitch.cpp
	src/core/decoder.cpp
	src/core/zero_crossing.cpp
	src/core/envelope.cpp
	src/core/voice.cpp
	src/core/paths.cpp
	src/core/wav_write.cpp
	src/core/sampler.cpp
)

add_library(one44_core STATIC ${ONE44_CORE_SOURCES})
set_target_properties(one44_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(one44_core PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_features(one44_core PUBLIC cxx_std_17)
if(UNIX AND NOT APPLE)
	target_link_libraries(one44_core PUBLIC pthread m)
endif()

set(ONE44_PLUGIN_SOURCES
	src/plugin/plugin.cpp
	src/gui/gui.cpp
)

if(WIN32)
	list(APPEND ONE44_PLUGIN_SOURCES src/gui/gui_win32.cpp src/plugin/one44.def)
elseif(UNIX AND NOT APPLE)
	list(APPEND ONE44_PLUGIN_SOURCES src/gui/gui_x11.cpp)
endif()

add_library(one44 MODULE ${ONE44_PLUGIN_SOURCES})
target_link_libraries(one44 PRIVATE one44_core clap)
target_include_directories(one44 PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set_target_properties(one44 PROPERTIES
	PREFIX ""
	OUTPUT_NAME "one44"
	CXX_VISIBILITY_PRESET default
	C_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)

if(UNIX AND NOT APPLE)
	target_link_libraries(one44 PRIVATE X11)
	target_link_options(one44 PRIVATE
		"-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/exports.map"
		"-Wl,-z,defs")
	set_target_properties(one44 PROPERTIES SUFFIX ".clap")
elseif(WIN32)
	set_target_properties(one44 PROPERTIES SUFFIX ".clap")
	target_link_libraries(one44 PRIVATE gdi32 user32)
	# Single-file drop-in for REAPER: do not require MinGW runtime DLLs on the user's PC.
	if(MINGW)
		target_link_options(one44 PRIVATE -static -static-libgcc -static-libstdc++ -Wl,--subsystem,windows)
	endif()
elseif(APPLE)
	set_target_properties(one44 PROPERTIES
		BUNDLE TRUE
		BUNDLE_EXTENSION clap
		OUTPUT_NAME "one44")
endif()

enable_testing()

set(ONE44_TEST_SOURCES
	tests/test_main.cpp
	tests/test_constraint.cpp
	tests/test_pitch.cpp
	tests/test_decoder.cpp
	tests/test_voice.cpp
	tests/test_wav_write.cpp
)

add_executable(one44_tests ${ONE44_TEST_SOURCES})
target_link_libraries(one44_tests PRIVATE one44_core)
target_include_directories(one44_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_definitions(one44_tests PRIVATE
	TEST_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixtures")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

add_executable(one44_clap_probe tests/clap_probe.cpp)
target_link_libraries(one44_clap_probe PRIVATE clap)
if(UNIX)
	target_link_libraries(one44_clap_probe PRIVATE dl)
endif()
if(MINGW)
	target_link_options(one44_clap_probe PRIVATE -static -static-libgcc -static-libstdc++)
endif()
target_include_directories(one44_clap_probe PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/third_party/clap/include)

if(NOT CMAKE_CROSSCOMPILING)
	add_test(NAME one44_unit_tests COMMAND one44_tests)
	add_test(NAME one44_clap_instantiate COMMAND one44_clap_probe $<TARGET_FILE:one44>)
	set_tests_properties(one44_clap_instantiate PROPERTIES DEPENDS one44)
else()
	find_program(ONE44_WINE wine)
	if(NOT ONE44_WINE)
		find_program(ONE44_WINE wine64)
	endif()
	if(ONE44_WINE)
		add_test(NAME one44_clap_instantiate
			COMMAND ${ONE44_WINE} one44_clap_probe.exe one44.clap)
		set_tests_properties(one44_clap_instantiate PROPERTIES
			DEPENDS one44
			WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
	endif()
endif()

if(WIN32)
	add_custom_command(TARGET one44 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist/windows-x64"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:one44> "${CMAKE_SOURCE_DIR}/dist/windows-x64/one44.clap"
		COMMENT "Copy Windows one44.clap into dist/windows-x64 for REAPER")
endif()

if(WIN32)
	add_custom_target(install-user
		COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{LOCALAPPDATA}/Programs/Common/CLAP"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:one44> "$ENV{LOCALAPPDATA}/Programs/Common/CLAP/one44.clap"
		DEPENDS one44
		COMMENT "Install one44.clap into %LOCALAPPDATA%\\Programs\\Common\\CLAP")
else()
	add_custom_target(install-user
		COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.clap"
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:one44> "$ENV{HOME}/.clap/one44.clap"
		DEPENDS one44
		COMMENT "Install one44.clap into ~/.clap for REAPER and other CLAP hosts")
endif()
