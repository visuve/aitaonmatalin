if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	# LibTorch completely explodes if the debug/release builds are mixed
	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
		FetchContent_Declare(Torch
			URL https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.9.0%2Bcpu.zip
			URL_HASH MD5=53b911e3eb52675e9ec520e93717912b)
	else()
		FetchContent_Declare(Torch
			URL https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-debug-2.9.0%2Bcpu.zip
			URL_HASH MD5=c23cd459272379c425edaf0a0722a438)
	endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	FetchContent_Declare(Torch
		URL https://download.pytorch.org/libtorch/cpu/libtorch-macos-arm64-2.9.0.zip
		URL_HASH MD5=175ba2e5a0eeab9cf2605ed5fcf686b6)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	FetchContent_Declare(Torch
		URL https://download.pytorch.org/libtorch/cpu/libtorch-shared-with-deps-2.9.0%2Bcpu.zip
		URL_HASH MD5=acb8e24c3fcbe1879195994f0189fb7a)
else()
	message(SEND_ERROR "Unsupported platform" )
endif()

FetchContent_MakeAvailable(Torch)
list(APPEND CMAKE_PREFIX_PATH "${torch_SOURCE_DIR}")

find_package(Torch REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

file(GLOB_RECURSE AITAONMATALIN_DQN_SRC "*.cpp" "*.hpp")

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	add_executable(aitaRL ${AITAONMATALIN_DQN_SRC})

	file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")

	add_custom_command(TARGET aitaRL
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		${TORCH_DLLS}
		$<TARGET_FILE_DIR:aitaRL>)
else()
	add_executable(aitaRL ${AITAONMATALIN_DQN_SRC})
endif()

target_link_libraries(aitaRL PRIVATE ${TORCH_LIBRARIES})
target_precompile_headers(aitaRL PUBLIC "AitaRL.pch")