cmake_minimum_required(VERSION 3.16)
project(aitaonmatalin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

include(FetchContent)
FetchContent_Declare(SFML
	GIT_REPOSITORY https://github.com/SFML/SFML.git
	GIT_TAG 3.0.x)
FetchContent_MakeAvailable(SFML)


file(GLOB_RECURSE AITAONMATALIN_SRC "src/*.cpp" "src/*.hpp")
add_executable(aitaonmatalin ${AITAONMATALIN_SRC})

target_precompile_headers(aitaonmatalin PUBLIC "src/Mega.pch")

target_include_directories(aitaonmatalin PRIVATE "src")
target_link_libraries(aitaonmatalin PRIVATE sfml-graphics)
