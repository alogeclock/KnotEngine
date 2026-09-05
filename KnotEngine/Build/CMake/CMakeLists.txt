cmake_minimum_required(VERSION 3.25)

# Visual Studio Output 창에서 vcpkg의 지역화된 메시지가 깨지지 않도록 영어 출력을 사용한다.
set(ENV{VSLANG} "1033")

# Visual Studio에서 사용할 빌드 구성을 세 가지로 제한한다.
set(CMAKE_CONFIGURATION_TYPES "Debug;Development;Shipping" CACHE STRING "KnotEngine build configurations" FORCE)

# KnotEngine 프로젝트를 만들고, C++와 Windows 리소스 컴파일러를 활성화한다.
project(KnotEngine VERSION 0.1.0 LANGUAGES CXX RC)

# CMake가 자동 생성하는 ALL_BUILD, ZERO_CHECK 등의 타깃을 하나의 솔루션 폴더로 묶는다.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

# 빌드 구성에 따라 컴파일러 및 링커 플래그를 설정한다.
# - Debug 구성은 디버깅 심볼을 포함하고 최적화를 비활성화한다.
# - Development 구성은 디버깅 심볼을 포함하고 일부 최적화를 활성화한다.
# - Shipping 구성은 디버깅 심볼을 제거하고 최적화를 활성화한다.
set(CMAKE_CXX_FLAGS_DEVELOPMENT "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "Compiler flags for Development builds" FORCE)
set(CMAKE_CXX_FLAGS_SHIPPING "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "Compiler flags for Shipping builds" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEVELOPMENT "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}" CACHE STRING "Linker flags for Development builds" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEVELOPMENT "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}" CACHE STRING "DLL linker flags for Development" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_SHIPPING "${CMAKE_SHARED_LINKER_FLAGS_RELEASE}" CACHE STRING "DLL linker flags for Shipping" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_SHIPPING "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "Linker flags for Shipping builds" FORCE)

# Development과 Shipping 구성에서 CMake가 가져온 라이브러리를 올바르게 매핑하도록 설정한다.
set(CMAKE_MAP_IMPORTED_CONFIG_DEVELOPMENT RelWithDebInfo Release)
set(CMAKE_MAP_IMPORTED_CONFIG_SHIPPING Release)

# 현재 CMakeLists.txt 위치에서 두 단계 위를 엔진 루트로 잡는다.
get_filename_component(KNOTENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# 제너레이터가 지정한 플랫폼을 우선 사용하고, 플랫폼 정보가 없다면 포인터 크기에 따라 64bit, 32bit를 결정한다.
if(CMAKE_GENERATOR_PLATFORM)
    set(KNOTENGINE_OUTPUT_PLATFORM "${CMAKE_GENERATOR_PLATFORM}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(KNOTENGINE_OUTPUT_PLATFORM "x64")
else()
    set(KNOTENGINE_OUTPUT_PLATFORM "Win32")
endif()

# 최종 실행 파일의 상위 출력 폴더를 만든다.
set(KNOTENGINE_BIN_DIR "${KNOTENGINE_ROOT}/Bin/${KNOTENGINE_OUTPUT_PLATFORM}")

# vcpkg 등의 툴체인 파일이 지정됐다면, 경로를 로그에 출력한다.
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using CMake toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Windows가 아닌 플랫폼에서는 프로젝트 생성을 중단한다.
if(NOT WIN32)
    message(FATAL_ERROR "KnotEngine currently targets Windows and DirectX 11.")
endif()

# Config 패키지 모드로 ImGui, nlohmann-json을 찾는다.
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# 세 개의 바이너리 타깃을 생성한다.
foreach(MODULE Engine Renderer Editor)
    file(GLOB_RECURSE ${MODULE}_SOURCES CONFIGURE_DEPENDS "${KNOTENGINE_ROOT}/Source/${MODULE}/*.cpp")
    file(GLOB_RECURSE ${MODULE}_HEADERS CONFIGURE_DEPENDS "${KNOTENGINE_ROOT}/Source/${MODULE}/*.h" "${KNOTENGINE_ROOT}/Source/${MODULE}/*.inl")
    if(MODULE STREQUAL "Editor")
        add_executable(${MODULE} WIN32 ${${MODULE}_SOURCES} ${${MODULE}_HEADERS} "${KNOTENGINE_ROOT}/Source/Resources/KnotEngine.rc" "${KNOTENGINE_ROOT}/Source/Resources/resource.h")
    else()
        add_library(${MODULE} SHARED ${${MODULE}_SOURCES} ${${MODULE}_HEADERS})
        string(TOUPPER "${MODULE}" MODULE_UPPER)
        target_compile_definitions(${MODULE} PRIVATE KNOT_${MODULE_UPPER}_EXPORTS)
    endif()

    target_compile_features(${MODULE} PUBLIC cxx_std_20)
    target_include_directories(${MODULE} PUBLIC "${KNOTENGINE_ROOT}/Source/${MODULE}")
    target_include_directories(${MODULE} PRIVATE "${KNOTENGINE_ROOT}")
    set(${MODULE}_PCH "${KNOTENGINE_ROOT}/Source/${MODULE}/${MODULE}PCH.h")
    target_precompile_headers(${MODULE} PRIVATE "${${MODULE}_PCH}")
    target_compile_definitions(${MODULE} PUBLIC
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        $<$<CONFIG:Debug>:KNOT_BUILD_DEBUG=1>
        $<$<CONFIG:Development>:KNOT_BUILD_DEVELOPMENT=1>
        $<$<CONFIG:Shipping>:KNOT_BUILD_SHIPPING=1>
    )
    target_compile_options(${MODULE} PRIVATE /utf-8 /MP /EHa /Zc:preprocessor)
    set_target_properties(${MODULE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${KNOTENGINE_BIN_DIR}/$<CONFIG>"
        ARCHIVE_OUTPUT_DIRECTORY "${KNOTENGINE_ROOT}/Intermediate/Lib/${KNOTENGINE_OUTPUT_PLATFORM}/$<CONFIG>"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        INTERPROCEDURAL_OPTIMIZATION_SHIPPING TRUE
    )
    source_group(TREE "${KNOTENGINE_ROOT}/Source/${MODULE}" PREFIX "Source" FILES ${${MODULE}_SOURCES} ${${MODULE}_HEADERS})
    source_group("Generated\\CMake" REGULAR_EXPRESSION ".*cmake_pch.*")
endforeach()

source_group("Source\\Resources" FILES
    "${KNOTENGINE_ROOT}/Source/Resources/KnotEngine.rc"
    "${KNOTENGINE_ROOT}/Source/Resources/resource.h"
)

target_link_libraries(Engine PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(Renderer PUBLIC Engine PRIVATE imgui::imgui d3d11 d3dcompiler dxgi)
target_link_libraries(Editor PRIVATE Engine Renderer imgui::imgui)

# 전역 new/delete만 각 바이너리에 포함하고, 실제 할당과 통계는 Engine.dll에서 공유한다.
target_sources(Renderer PRIVATE "${KNOTENGINE_ROOT}/Source/Engine/Core/Memory/MemoryOperators.cpp")
target_sources(Editor PRIVATE "${KNOTENGINE_ROOT}/Source/Engine/Core/Memory/MemoryOperators.cpp")

target_compile_definitions(Editor PRIVATE WITH_EDITOR=1)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Editor)
set_target_properties(Editor PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Editor>")

include("${CMAKE_CURRENT_LIST_DIR}/Reflection.cmake")

# 데이터만 수정해도 배치한다. 실행 중 변경되는 설정은 Saved에 저장한다.
add_custom_target(RUNTIME_FILES
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${KNOTENGINE_ROOT}/Contents" "${KNOTENGINE_BIN_DIR}/$<CONFIG>/Contents"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${KNOTENGINE_ROOT}/Settings" "${KNOTENGINE_BIN_DIR}/$<CONFIG>/Settings"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${KNOTENGINE_ROOT}/Shaders" "${KNOTENGINE_BIN_DIR}/$<CONFIG>/Shaders"
    COMMENT "Staging runtime contents, settings and shaders"
    VERBATIM
)
set_target_properties(RUNTIME_FILES PROPERTIES FOLDER "CMake")
add_dependencies(Editor RUNTIME_FILES)
foreach(DATA_DIR Contents Settings Shaders)
    file(GLOB_RECURSE RUNTIME_FILES CONFIGURE_DEPENDS "${KNOTENGINE_ROOT}/${DATA_DIR}/*")
    set_source_files_properties(${RUNTIME_FILES} PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE "None")
    if(DATA_DIR STREQUAL "Shaders")
        target_sources(Renderer PRIVATE ${RUNTIME_FILES})
    else()
        target_sources(Editor PRIVATE ${RUNTIME_FILES})
    endif()
    source_group(TREE "${KNOTENGINE_ROOT}/${DATA_DIR}" PREFIX "${DATA_DIR}" FILES ${RUNTIME_FILES})
endforeach()
