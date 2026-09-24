# Read declarations using the same target include paths and build definitions as MSVC.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sys; sys.path.insert(0, r'${KNOTENGINE_ROOT}/../Scripts'); from Toolchain import ensure_reflection_tools; ensure_reflection_tools()"
    RESULT_VARIABLE KNOTENGINE_REFLECTION_TOOLS_RESULT
    COMMAND_ERROR_IS_FATAL ANY
)

# Reflection 마커를 선언한 헤더만 Clang 입력으로 사용하고 원본 폴더 구조대로 생성 파일을 배치한다.
foreach(MODULE Engine Editor)
    set(${MODULE}_REFLECTION_HEADERS)
    foreach(HEADER IN LISTS ${MODULE}_HEADERS)
        file(STRINGS "${HEADER}" REFLECTION_MARKER LIMIT_COUNT 1 REGEX "^[ \t]*(UCLASS|USTRUCT|UENUM)[ \t]*\\(")
        if(REFLECTION_MARKER)
            list(APPEND ${MODULE}_REFLECTION_HEADERS "${HEADER}")
        endif()
        unset(REFLECTION_MARKER)
    endforeach()

    set(REFLECTION_DIR "${KNOTENGINE_ROOT}/Intermediate/Reflection/${MODULE}")
    set(REFLECTION_HEADERS "${REFLECTION_DIR}/Headers.txt")
    file(MAKE_DIRECTORY "${REFLECTION_DIR}")
    string(REPLACE ";" "\n" HEADER_LINES "${${MODULE}_REFLECTION_HEADERS}")
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

    set(REFLECTION_OUTPUTS)
    set(REFLECTION_RELATIVE_OUTPUTS)
    foreach(HEADER IN LISTS ${MODULE}_REFLECTION_HEADERS)
        file(RELATIVE_PATH RELATIVE_HEADER "${KNOTENGINE_ROOT}/Source/${MODULE}" "${HEADER}")
        string(REGEX REPLACE "\\.[^.]+$" ".gen.cpp" RELATIVE_OUTPUT "${RELATIVE_HEADER}")
        list(APPEND REFLECTION_RELATIVE_OUTPUTS "${RELATIVE_OUTPUT}")
        list(APPEND REFLECTION_OUTPUTS "${REFLECTION_DIR}/$<CONFIG>/${RELATIVE_OUTPUT}")
    endforeach()
    list(APPEND REFLECTION_OUTPUTS "${REFLECTION_DIR}/$<CONFIG>/Registration.gen.cpp")

    set(DEPENDENCY_ARGUMENTS)
    set(REFLECTION_DEPENDENCIES ${${MODULE}_HEADERS})
    if(MODULE STREQUAL "Editor")
        list(APPEND DEPENDENCY_ARGUMENTS --dependencies "${KNOTENGINE_ROOT}/Intermediate/Reflection/Engine/Headers.txt")
        list(APPEND REFLECTION_DEPENDENCIES ${Engine_HEADERS})
    endif()

    set(REFLECTION_STAMP "${REFLECTION_DIR}/$<CONFIG>/Reflection.stamp")
    add_custom_command(
        OUTPUT "${REFLECTION_STAMP}" ${REFLECTION_OUTPUTS}
        BYPRODUCTS "${REFLECTION_DIR}/$<CONFIG>/ReflectionState.json"
        COMMAND "${Python3_EXECUTABLE}" "${KNOTENGINE_ROOT}/../Scripts/KnotHeaderTool.py"
            --module ${MODULE}
            --environment "${REFLECTION_DIR}/$<CONFIG>/Environment.txt"
            --headers "${REFLECTION_HEADERS}"
            --stamp "${REFLECTION_STAMP}"
            ${DEPENDENCY_ARGUMENTS}
        DEPENDS
            ${REFLECTION_DEPENDENCIES}
            "${REFLECTION_HEADERS}"
            "${REFLECTION_DIR}/$<CONFIG>/Environment.txt"
            "${KNOTENGINE_ROOT}/../Scripts/KnotHeaderTool.py"
            "${KNOTENGINE_ROOT}/../Scripts/Toolchain.py"
        COMMENT "Generating ${MODULE} reflection declarations"
        VERBATIM
    )
    set_source_files_properties("${REFLECTION_STAMP}" PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE)
    set_source_files_properties(${REFLECTION_OUTPUTS} PROPERTIES GENERATED TRUE)
    target_sources(${MODULE} PRIVATE "${REFLECTION_STAMP}" ${REFLECTION_OUTPUTS})
    foreach(CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
        set(CONFIG_OUTPUTS)
        foreach(RELATIVE_OUTPUT IN LISTS REFLECTION_RELATIVE_OUTPUTS)
            list(APPEND CONFIG_OUTPUTS "${REFLECTION_DIR}/${CONFIG}/${RELATIVE_OUTPUT}")
        endforeach()
        list(APPEND CONFIG_OUTPUTS "${REFLECTION_DIR}/${CONFIG}/Registration.gen.cpp")
        source_group(TREE "${REFLECTION_DIR}/${CONFIG}" PREFIX "Generated\\Reflection\\${CONFIG}" FILES ${CONFIG_OUTPUTS})
    endforeach()
endforeach()
