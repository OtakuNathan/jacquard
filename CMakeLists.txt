cmake_minimum_required(VERSION 3.20)
project(jacquard LANGUAGES CXX)

add_library(jacquard INTERFACE)
add_library(jacquard::jacquard ALIAS jacquard)
target_include_directories(jacquard INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(jacquard INTERFACE cxx_std_14)

include(cmake/JacquardTla.cmake)

# Consumers of the header-only library need neither Python nor Java, and do
# not inherit the example targets or jacquard's own tests.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  return()
endif()

include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(spec)
add_custom_target(check-spec)

function(jacquard_example name definition terminals)
  jacquard_add_emitter(emit_${name} "${definition}")
  add_custom_target(spec_${name}
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/tla.py" generate
            --emitter "$<TARGET_FILE:emit_${name}>" --output "${PROJECT_SOURCE_DIR}/spec"
    DEPENDS emit_${name} VERBATIM)
  add_custom_target(check_spec_${name}
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/tla.py" generate
            --emitter "$<TARGET_FILE:emit_${name}>" --output "${PROJECT_SOURCE_DIR}/spec" --check
    DEPENDS emit_${name} VERBATIM)
  add_dependencies(spec spec_${name})
  add_dependencies(check-spec check_spec_${name})
  if(BUILD_TESTING)
    add_test(NAME spec.${name}
      COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/tla.py" generate
              --emitter "$<TARGET_FILE:emit_${name}>" --output "${PROJECT_SOURCE_DIR}/spec"
              --check --terminals "${terminals}")
    set_tests_properties(spec.${name} PROPERTIES LABELS generator)
    jacquard_add_tla_test(NAME tla.${name} EMITTER emit_${name})
  endif()
endfunction()

# One registration per machine: BEGIN_MACHINE(<Name>) determines output file names.
jacquard_example(job "jacquard/transitions.inc" "{\"Done\", \"Cancelled\"}")
jacquard_example(conn "jacquard/conn.inc" "{}")
jacquard_example(review "jacquard/review.inc" "{\"Approved\", \"Rejected\", \"Cancelled\"}")
add_subdirectory(examples/guarded_review)

if(BUILD_TESTING)
  add_subdirectory(tests)
  if(NOT TLA2TOOLS_JAR)
    message(STATUS "TLC tests disabled; set -DTLA2TOOLS_JAR=/path/to/tla2tools.jar to enable them.")
  endif()
endif()
