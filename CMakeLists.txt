cmake_minimum_required(VERSION 3.25)

project(a.log LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(MARCH_NATIVE "add_compile_options(-march=native)" ON)

if(
	(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Release")
	AND (
		"${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
		OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
	)
)
	if(True)
		if(NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
			add_compile_options(-fsanitize=object-size)
			add_link_options(-fsanitize=object-size)
		endif()

		add_compile_options(-fsanitize=address,leak,undefined,alignment,bounds)
		add_link_options(-fsanitize=address,leak,undefined,alignment,bounds)
	else()
		add_compile_options(-fsanitize=thread)
		add_link_options(-fsanitize=thread)
	endif()
endif()

if(WIN32)
	add_definitions("-DNOMINMAX")
	add_definitions("-DWIN32_LEAN_AND_MEAN")
endif()

if(
	"${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"
	OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
)
	add_compile_options(
		$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
		$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
		-Wall
		-Wconversion
		-Wextra
		-Wno-sign-conversion
		-Wpedantic
		$<IF:$<BOOL:${MARCH_NATIVE}>,-march=native,>
		-pipe
	)
elseif(MSVC)
	add_compile_options(
		/D_HAS_EXCEPTIONS=0 # -fno-exceptions
		/GR- # -fno-rtti
		/permissive-
		/utf-8
		/wd4996
	)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
if(${IPO_SUPPORTED})
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

include(third_party/cmrc/CMakeRC.cmake)
file(GLOB WWW "www/*")
cmrc_add_resource_library(rc "${WWW}")

add_definitions("-DCPPHTTPLIB_NO_EXCEPTIONS")
include_directories(SYSTEM "third_party/cpp-httplib")

find_package(ICU COMPONENTS dt uc REQUIRED)

find_package(re2)

include_directories(SYSTEM "third_party/rapidjson/include")

include_directories(SYSTEM "third_party/StringZilla/include")

add_definitions("-DUTF_CPP_CPLUSPLUS=201103L")
include_directories(SYSTEM "third_party/utfcpp/source")

add_executable(a.log main.cpp
)

target_link_libraries(a.log
	ICU::dt
	ICU::uc
)

if(${re2_FOUND})
	add_definitions("-DUSE_REGEX")
	target_link_libraries(a.log re2::re2)
endif()

target_link_libraries(a.log
	rc
)

install(TARGETS a.log DESTINATION .)
