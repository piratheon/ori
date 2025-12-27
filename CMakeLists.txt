cmake_minimum_required(VERSION 3.10)
project(OriAssistant)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define installation data directory
set(ORI_DATA_DIR "/usr/share/ori")
add_definitions(-DORI_DATA_DIR="${ORI_DATA_DIR}")

# Add include directories
include_directories(include include/external)

# Source files
set(SOURCES
    src/main.cpp
    src/core/ori_core.cpp
    src/core/ori_config.cpp
    src/core/ori_edit.cpp
    src/plugins/orpm.cpp
    src/gui/gui.cpp
)

# Create executable
add_executable(ori ${SOURCES})

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(CURL REQUIRED libcurl)

# Include directories and compile definitions
if(JSONCPP_FOUND)
    target_include_directories(ori PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_compile_definitions(ori PRIVATE JSONCPP_FOUND)
else()
    message(FATAL_ERROR "jsoncpp is required to build ORI")
endif()

if(CURL_FOUND)
    target_include_directories(ori PRIVATE ${CURL_INCLUDE_DIRS})
    target_compile_definitions(ori PRIVATE CURL_FOUND)
else()
    message(FATAL_ERROR "libcurl is required to build ORI")
endif()

# Link libraries
target_link_libraries(ori PRIVATE ${JSONCPP_LIBRARIES} ${CURL_LIBRARIES} stdc++fs pthread)

# Additional libraries that might be needed
# find_package(CURL)
# if(CURL_FOUND)
#     target_link_libraries(ori ${CURL_LIBRARIES})
# endif()

message(STATUS "Meld is an optional dependency for graphical diffs. Install it for enhanced compare functionality.")

# Copy system prompt into build directory


# Install targets
install(TARGETS ori DESTINATION bin)
install(FILES orpm DESTINATION bin)

# Install additional data files for packaging
install(DIRECTORY plugins/ DESTINATION share/Ori/plugins OPTIONAL)
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
# Embed chart.js into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/chart.js.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 chart.js ${CMAKE_CURRENT_BINARY_DIR}/chart.js.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/chart.js
)
# Embed css2.css into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/css2.css.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 css2.css ${CMAKE_CURRENT_BINARY_DIR}/css2.css.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/css2.css
)
# Embed marked.min.js into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/marked.min.js.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 marked.min.js ${CMAKE_CURRENT_BINARY_DIR}/marked.min.js.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/marked.min.js
)
# Embed all.min.css into the executable
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/all.min.css.o
    COMMAND objcopy --input-target binary --output-target elf64-x86-64 --binary-architecture i386:x86-64 all.min.css ${CMAKE_CURRENT_BINARY_DIR}/all.min.css.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/www/all.min.css
)
target_sources(ori PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/favicon.svg.o)
# Provide a CMake install config (optional simple staging)
include(GNUInstallDirs)
