# Read declarations using the same target include paths and build definitions as MSVC.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sys; sys.path.insert(0, r'${KNOTENGINE_ROOT}/../Scripts'); from Toolchain import ensure_reflection_tools; ensure_reflection_tools()"
    RESULT_VARIABLE KNOTENGINE_REFLECTION_TOOLS_RESULT
    COMMAND_ERROR_IS_FATAL ANY
)

# 각 모듈의 환경과 생성 파일을 분리한다.
foreach(MODULE Engine Editor)
    set(REFLECTION_DIR "${KNOTENGINE_ROOT}/Intermediate/Reflection/${MODULE}")
    set(REFLECTION_HEADERS "${REFLECTION_DIR}/Headers.txt")
    file(MAKE_DIRECTORY "${REFLECTION_DIR}")
    string(REPLACE ";" "\n" HEADER_LINES "${${MODULE}_HEADERS}")
    file(CONFIGURE OUTPUT "${REFLECTION_HEADERS}" CONTENT "@HEADER_LINES@\n" @ONLY)
    file(GENERATE OUTPUT "${REFLECTION_DIR}/$<CONFIG>/Environment.txt" CONTENT
"module=${MODULE}
source=${KNOTENGINE_ROOT}/Source
compiler=${CMAKE_CXX_COMPILER}
compiler_version=${CMAKE_CXX_COMPILER_VERSION}
configuration=$<CONFIG>
flags=${CMAKE_CXX_FLAGS} $<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG}>$<$<CONFIG:Development>:${CMAKE_CXX_FLAGS_DEVELOPMENT}>$<$<CONFIG:Shipping>:${CMAKE_CXX_FLAGS_SHIPPING}>
sdk=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}
pch=${${MODULE}_PCH}
include=$<JOIN:$<TARGET_PROPERTY:${MODULE},INCLUDE_DIRECTORIES>,\ninclude=>
define=$<JOIN:$<TARGET_PROPERTY:${MODULE},COMPILE_DEFINITIONS>,\ndefine=>
")
    set(DEPENDENCY_ARGUMENTS)
    if(MODULE STREQUAL "Editor")
        list(APPEND DEPENDENCY_ARGUMENTS --dependencies "${KNOTENGINE_ROOT}/Intermediate/Reflection/Engine/Headers.txt")
    endif()
    set(REFLECTION_OUTPUT "${REFLECTION_DIR}/$<CONFIG>/Reflection.gen.cpp")
    add_custom_command(TARGET ${MODULE} PRE_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${KNOTENGINE_ROOT}/../Scripts/KnotHeaderTool.py"
            --module ${MODULE}
            --environment "${REFLECTION_DIR}/$<CONFIG>/Environment.txt"
            --headers "${REFLECTION_HEADERS}"
            ${DEPENDENCY_ARGUMENTS}
        BYPRODUCTS ${REFLECTION_OUTPUT}
        COMMENT "Checking ${MODULE} reflection declarations"
        VERBATIM
    )
    set_target_properties(${MODULE} PROPERTIES VS_GLOBAL_DisableFastUpToDateCheck true)
    target_sources(${MODULE} PRIVATE ${REFLECTION_OUTPUT})
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        source_group("Generated\\Reflection\\${CONFIG}" FILES "${REFLECTION_DIR}/${CONFIG}/Reflection.gen.cpp")
    endforeach()
endforeach()
