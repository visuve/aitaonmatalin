if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	# LibTorch completely explodes if the debug/release builds are mixed
	if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
		FetchContent_Declare(Torch
			URL https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.9.1%2Bcpu.zip
			URL_HASH MD5=4fffc238c66565a8e8836def850c06d5)
	else()
		FetchContent_Declare(Torch
			URL https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-debug-2.9.1%2Bcpu.zip
			URL_HASH MD5=3b4b6b5f6bf986693d9e6bb1233caf3f)
	endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
	FetchContent_Declare(Torch
		URL https://download.pytorch.org/libtorch/cpu/libtorch-macos-arm64-2.9.1.zip
		URL_HASH MD5=096da935727ffac060b1962da892d6da)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
	FetchContent_Declare(Torch
		URL https://download.pytorch.org/libtorch/cpu/libtorch-shared-with-deps-2.9.1%2Bcpu.zip
		URL_HASH MD5=63cbda340ef91c7d6c140986bf2fa136)
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