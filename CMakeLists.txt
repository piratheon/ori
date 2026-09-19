cmake_minimum_required(VERSION 3.10)

# Resolve project version:
# Priority 1: Environment variable ORI_VERSION or VERSION
# Priority 2: File .version in source root
# Priority 3: Fallback "0.0"
if(DEFINED ENV{ORI_VERSION} AND NOT "$ENV{ORI_VERSION}" STREQUAL "")
    set(ORI_VERSION "$ENV{ORI_VERSION}")
elseif(DEFINED ENV{VERSION} AND NOT "$ENV{VERSION}" STREQUAL "")
    set(ORI_VERSION "$ENV{VERSION}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.version")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.version" ORI_VER_RAW)
    string(STRIP "${ORI_VER_RAW}" ORI_VER_CLEAN)
    if(NOT "${ORI_VER_CLEAN}" STREQUAL "")
        set(ORI_VERSION "${ORI_VER_CLEAN}")
    else()
        set(ORI_VERSION "0.0")
    endif()
else()
    set(ORI_VERSION "0.0")
endif()

# Clean version for CMake project declaration (digits and dots)
string(REGEX REPLACE "^[vV]" "" ORI_SEMVER "${ORI_VERSION}")
if(NOT ORI_SEMVER MATCHES "^[0-9]")
    set(ORI_SEMVER "0.0")
endif()

project(OriAssistant VERSION ${ORI_SEMVER})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define installation data directory and version string
set(ORI_DATA_DIR "/usr/share/ori")
add_definitions(-DORI_DATA_DIR="${ORI_DATA_DIR}")
add_definitions(-DORI_VERSION="${ORI_VERSION}")

# Add include directories
include_directories(include include/external)

# Option for embedded JsonCpp
option(USE_STATIC_JSONCPP "Embed JsonCpp directly into executable for maximum portability" ON)

# Source files
set(SOURCES
    src/main.cpp
    src/core/ori_core.cpp
    src/core/ori_config.cpp
    src/core/ori_edit.cpp
    src/core/ori_migration.cpp
    src/core/ori_provider.cpp
    src/core/ori_agents.cpp
    src/core/ori_task.cpp
    src/core/ori_skills.cpp
    src/gui/gui.cpp
)

if(USE_STATIC_JSONCPP)
    message(STATUS "Using embedded static JsonCpp for portable build (ORI_VERSION: ${ORI_VERSION})")
    list(APPEND SOURCES src/external/jsoncpp.cpp)
    add_definitions(-DJSONCPP_FOUND)
endif()

# Create executable
add_executable(ori ${SOURCES})

# Find required packages
find_package(PkgConfig REQUIRED)

if(NOT USE_STATIC_JSONCPP)
    pkg_check_modules(JSONCPP REQUIRED jsoncpp)
    if(JSONCPP_FOUND)
        target_include_directories(ori PRIVATE ${JSONCPP_INCLUDE_DIRS})
        target_compile_definitions(ori PRIVATE JSONCPP_FOUND)
        target_link_libraries(ori PRIVATE ${JSONCPP_LIBRARIES})
    else()
        message(FATAL_ERROR "jsoncpp is required to build ORI")
    endif()
endif()

pkg_check_modules(CURL REQUIRED libcurl)
if(CURL_FOUND)
    target_include_directories(ori PRIVATE ${CURL_INCLUDE_DIRS})
    target_compile_definitions(ori PRIVATE CURL_FOUND)
    target_link_libraries(ori PRIVATE ${CURL_LIBRARIES})
else()
    message(FATAL_ERROR "libcurl is required to build ORI")
endif()

# Link standard libraries
target_link_libraries(ori PRIVATE stdc++fs pthread)

message(STATUS "Meld is an optional dependency for graphical diffs. Install it for enhanced compare functionality.")

# Install targets
install(TARGETS ori DESTINATION bin)

# Install additional data files for packaging
install(FILES LICENSE DESTINATION share/Ori)
install(DIRECTORY include/ DESTINATION include/ori OPTIONAL)
install(DIRECTORY www/ DESTINATION share/Ori/www OPTIONAL)

# Embed index.html into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index.html.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 index.html ${CMAKE_CURRENT_BINARY_DIR}/index.html.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/index.html
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/index.html.o)

# Embed favicon.svg into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/favicon.svg.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 favicon.svg ${CMAKE_CURRENT_BINARY_DIR}/favicon.svg.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/favicon.svg
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/favicon.svg.o)

# Embed tw.js into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tw.js.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 tw.js ${CMAKE_CURRENT_BINARY_DIR}/tw.js.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/tw.js
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/tw.js.o)

# Embed chart.js into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/chart.js.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 chart.js ${CMAKE_CURRENT_BINARY_DIR}/chart.js.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www/lib
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/lib/chart.js
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/chart.js.o)

# Embed css2.css into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/css2.css.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 css2.css ${CMAKE_CURRENT_BINARY_DIR}/css2.css.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/css2.css
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/css2.css.o)

# Embed marked.min.js into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/marked.min.js.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 marked.min.js ${CMAKE_CURRENT_BINARY_DIR}/marked.min.js.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www/lib
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/lib/marked.min.js
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/marked.min.js.o)

# Embed all.min.css into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/all.min.css.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 all.min.css ${CMAKE_CURRENT_BINARY_DIR}/all.min.css.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www/css
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/css/all.min.css
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/all.min.css.o)

# Provide a CMake install config
include(GNUInstallDirs)
