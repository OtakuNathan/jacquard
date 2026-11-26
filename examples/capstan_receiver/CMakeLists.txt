cmake_minimum_required(VERSION 3.20)
project(jacquard_capstan_receiver LANGUAGES CXX)

include(CTest)
add_subdirectory("../.." jacquard)
include("../../cmake/JacquardTla.cmake")

set(CAPSTAN_SOURCE_DIR "" CACHE PATH "Optional checkout for receiver correspondence tests")

jacquard_add_emitter(receiver_emitter "${CMAKE_CURRENT_SOURCE_DIR}/receiver.inc")
jacquard_add_emitter(receiver_restart_mutant "${CMAKE_CURRENT_SOURCE_DIR}/receiver.inc" RECEIVER_RESTART_MUTANT)

if(BUILD_TESTING)
  add_executable(receiver_model_test model_test.cpp)
  target_link_libraries(receiver_model_test PRIVATE jacquard::jacquard)
  target_include_directories(receiver_model_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  jacquard_strict_cxx14(receiver_model_test)
  add_test(NAME receiver.runtime COMMAND receiver_model_test)
  set_tests_properties(receiver.runtime PROPERTIES LABELS runtime)

  if(CAPSTAN_SOURCE_DIR)
    if(NOT EXISTS "${CAPSTAN_SOURCE_DIR}/include/capstan/migration/transfer.hpp")
      message(FATAL_ERROR "CAPSTAN_SOURCE_DIR must name a Capstan checkout")
    endif()
    add_executable(receiver_correspondence correspondence.cpp)
    target_link_libraries(receiver_correspondence PRIVATE jacquard::jacquard)
    target_include_directories(receiver_correspondence PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CAPSTAN_SOURCE_DIR}/include")
    jacquard_strict_cxx14(receiver_correspondence)
    add_test(NAME receiver.correspondence COMMAND receiver_correspondence)
    set_tests_properties(receiver.correspondence PROPERTIES LABELS "runtime;correspondence")
  endif()

  jacquard_add_tla_test(NAME receiver.safe EMITTER receiver_emitter
    MODULE ReceiverProperties.tla CONFIG Safe.cfg)
  jacquard_add_tla_test(NAME receiver.early_seal_mutant EMITTER receiver_emitter
    MODULE ReceiverProperties.tla CONFIG EarlySeal.cfg EXPECT_INVARIANT PreparedComplete)
  jacquard_add_tla_test(NAME receiver.restart_mutant EMITTER receiver_restart_mutant
    MODULE ReceiverProperties.tla CONFIG Safe.cfg EXPECT_INVARIANT AbortIsTerminal)
  jacquard_add_tla_test(NAME receiver.prepared_witness EMITTER receiver_emitter
    MODULE ReceiverProperties.tla CONFIG PreparedWitness.cfg EXPECT_INVARIANT NeverPrepared KIND witness)
  jacquard_add_tla_test(NAME receiver.abort_during_validation_witness EMITTER receiver_emitter
    MODULE ReceiverProperties.tla CONFIG AbortDuringValidation.cfg
    EXPECT_INVARIANT NeverAbortDuringValidation KIND witness)
  jacquard_add_tla_test(NAME receiver.out_of_order_witness EMITTER receiver_emitter
    MODULE ReceiverProperties.tla CONFIG OutOfOrder.cfg EXPECT_INVARIANT NeverOutOfOrder KIND witness)
  if(NOT TLA2TOOLS_JAR)
    message(STATUS "Set TLA2TOOLS_JAR to activate receiver model checking")
  endif()
endif()
