cmake_minimum_required(VERSION 3.16)
project(organizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# --- Executable ---
add_executable(organizer
  src/main.cpp
  src/scanner.cpp
  src/hashing.cpp
  src/executor.cpp
)

target_include_directories(organizer PRIVATE include)
target_link_libraries(organizer PRIVATE nlohmann_json::nlohmann_json)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(organizer PRIVATE -Wall -Wextra)
endif()
