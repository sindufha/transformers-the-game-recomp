cmake_minimum_required(VERSION 3.22)

# This target checks the lifted C on the destination architecture. A static
# archive does not supply the Windows APIs or resolve the host runtime symbols.
set(TRANSFORMERS_LIFT_DIR "" CACHE PATH "Directory containing lift-summary.json and gen/")
set(PCRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/pcrecomp" CACHE PATH "Pinned pcrecomp checkout")
get_filename_component(TRANSFORMERS_LIFT_DIR "${TRANSFORMERS_LIFT_DIR}" ABSOLUTE
    BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT EXISTS "${TRANSFORMERS_LIFT_DIR}/lift-summary.json")
    message(FATAL_ERROR "Set TRANSFORMERS_LIFT_DIR to the output from tools/lift.py")
endif()
file(READ "${TRANSFORMERS_LIFT_DIR}/lift-summary.json" LIFT_SUMMARY)
string(JSON LIFT_STATUS GET "${LIFT_SUMMARY}" status)
if(NOT LIFT_STATUS STREQUAL "code_generated")
    message(FATAL_ERROR "Lift is incomplete. Inspect lift-summary.json before compiling.")
endif()
if(NOT EXISTS "${PCRECOMP_DIR}/runtime/recomp32/recomp_types.h")
    message(FATAL_ERROR "Pinned pcrecomp runtime header is missing")
endif()

project(TransformersRecompiled LANGUAGES C)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(STRICT_RUNTIME "${CMAKE_CURRENT_BINARY_DIR}/strict-runtime")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/prepare_runtime.py"
    --lift-dir "${TRANSFORMERS_LIFT_DIR}" --out "${STRICT_RUNTIME}"
    RESULT_VARIABLE PREPARE_RESULT)
if(NOT PREPARE_RESULT EQUAL 0)
    message(FATAL_ERROR "Preparing the pinned strict runtime failed")
endif()

file(GLOB LIFT_SOURCES CONFIGURE_DEPENDS "${TRANSFORMERS_LIFT_DIR}/gen/recomp_*.c")
list(LENGTH LIFT_SOURCES SOURCE_COUNT)
if(SOURCE_COUNT LESS 2)
    message(FATAL_ERROR "No complete generated source set")
endif()
add_library(transformers_lift STATIC ${LIFT_SOURCES})
target_include_directories(transformers_lift PRIVATE
    "${STRICT_RUNTIME}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    "${TRANSFORMERS_LIFT_DIR}/gen"
)
set_target_properties(transformers_lift PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_options(transformers_lift PRIVATE -fmax-type-align=1)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(transformers_lift PRIVATE
        -include "${TRANSFORMERS_LIFT_DIR}/gen/recomp_funcs.h"
        -fno-strict-aliasing -fwrapv -ffp-contract=off
    )
endif()
message(STATUS "Building a lifted-code archive only; Android game runtime is not implemented")
