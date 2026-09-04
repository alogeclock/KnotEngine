# Read declarations using the same target include paths and build definitions as MSVC.
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" -c "import sys; sys.path.insert(0, r'${KNOTENGINE_ROOT}/../Scripts'); from Toolchain import ensure_reflection_tools; ensure_reflection_tools()"
    RESULT_VARIABLE KNOTENGINE_REFLECTION_TOOLS_RESULT
    COMMAND_ERROR_IS_FATAL ANY
)

set(KNOTENGINE_REFLECTION_DIR "${KNOTENGINE_ROOT}/Intermediate/Reflection")
set(KNOTENGINE_REFLECTION_HEADERS "${KNOTENGINE_REFLECTION_DIR}/Headers.txt")
file(MAKE_DIRECTORY "${KNOTENGINE_REFLECTION_DIR}")
string(REPLACE ";" "\n" KNOTENGINE_REFLECTION_HEADER_LINES "${KNOTENGINE_HEADER_FILES}")
file(CONFIGURE OUTPUT "${KNOTENGINE_REFLECTION_HEADERS}" CONTENT "@KNOTENGINE_REFLECTION_HEADER_LINES@\n" @ONLY)

file(GENERATE OUTPUT "${KNOTENGINE_REFLECTION_DIR}/$<CONFIG>/Environment.txt" CONTENT
"source=${KNOTENGINE_ROOT}/Source
compiler=${CMAKE_CXX_COMPILER}
compiler_version=${CMAKE_CXX_COMPILER_VERSION}
configuration=$<CONFIG>
flags=${CMAKE_CXX_FLAGS} $<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG}>$<$<CONFIG:Development>:${CMAKE_CXX_FLAGS_DEVELOPMENT}>$<$<CONFIG:Shipping>:${CMAKE_CXX_FLAGS_SHIPPING}>
sdk=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}
pch=${KNOTENGINE_ROOT}/pch.h
include=$<JOIN:$<TARGET_PROPERTY:KnotEngine,INCLUDE_DIRECTORIES>,\ninclude=>
define=$<JOIN:$<TARGET_PROPERTY:KnotEngine,COMPILE_DEFINITIONS>,\ndefine=>
")

set(KNOTENGINE_REFLECTION_OUTPUTS "${KNOTENGINE_REFLECTION_DIR}/$<CONFIG>/Reflection.gen.cpp")

# Run the content/dependency check on every IDE build. Unchanged generated files are not rewritten.
add_custom_command(TARGET KnotEngine PRE_BUILD
    COMMAND "${Python3_EXECUTABLE}" "${KNOTENGINE_ROOT}/../Scripts/KnotHeaderTool.py"
        --environment "${KNOTENGINE_REFLECTION_DIR}/$<CONFIG>/Environment.txt"
        --headers "${KNOTENGINE_REFLECTION_HEADERS}"
    BYPRODUCTS ${KNOTENGINE_REFLECTION_OUTPUTS}
    COMMENT "Checking reflection declarations and generating registration"
    VERBATIM
)
set_target_properties(KnotEngine PROPERTIES VS_GLOBAL_DisableFastUpToDateCheck true)
target_sources(KnotEngine PRIVATE ${KNOTENGINE_REFLECTION_OUTPUTS})
# source_group needs concrete paths; it does not expand $<CONFIG>.
foreach(KNOTENGINE_REFLECTION_CONFIG IN LISTS CMAKE_CONFIGURATION_TYPES)
    source_group("Generated\\Reflection\\${KNOTENGINE_REFLECTION_CONFIG}" FILES
        "${KNOTENGINE_REFLECTION_DIR}/${KNOTENGINE_REFLECTION_CONFIG}/Reflection.gen.cpp"
    )
endforeach()
