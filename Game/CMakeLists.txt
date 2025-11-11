include(FetchContent)
FetchContent_Declare(SFML
	GIT_REPOSITORY https://github.com/SFML/SFML.git
	GIT_TAG 3.0.x)
FetchContent_MakeAvailable(SFML)

file(GLOB_RECURSE AITAONMATALIN_SRC "*.cpp" "*.hpp")

if(CMAKE_SYSTEM_NAME MATCHES "Windows" AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_executable(aitaonmatalin WIN32 ${AITAONMATALIN_SRC})
else()
	add_executable(aitaonmatalin ${AITAONMATALIN_SRC})
endif()

target_precompile_headers(aitaonmatalin PUBLIC "Aita.pch")

if(CMAKE_SYSTEM_NAME MATCHES "Windows" AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_link_libraries(aitaonmatalin PRIVATE SFML::Graphics SFML::Main)
else()
	target_link_libraries(aitaonmatalin PRIVATE SFML::Graphics)
endif()