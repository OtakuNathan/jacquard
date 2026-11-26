function(jacquard_runtime_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE jacquard::jacquard)
  target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  jacquard_strict_cxx14(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS runtime)
endfunction()

jacquard_runtime_test(test_machine test_machine.cpp)
jacquard_runtime_test(strict_ok strict_ok.cpp)
jacquard_runtime_test(link_odr link_odr_a.cpp link_odr_b.cpp)
jacquard_runtime_test(test_contracts test_contracts.cpp)
jacquard_runtime_test(test_hook_results test_hook_results.cpp)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-fno-exceptions JACQUARD_HAS_FNO_EXCEPTIONS)
if(JACQUARD_HAS_FNO_EXCEPTIONS)
  jacquard_runtime_test(test_machine_noexcept test_machine.cpp)
  jacquard_runtime_test(strict_ok_noexcept strict_ok.cpp)
  jacquard_runtime_test(link_odr_noexcept link_odr_a.cpp link_odr_b.cpp)
  jacquard_runtime_test(test_hook_results_noexcept test_hook_results.cpp)
  foreach(name IN ITEMS test_machine_noexcept strict_ok_noexcept link_odr_noexcept test_hook_results_noexcept)
    target_compile_options(${name} PRIVATE -fno-exceptions)
    set_tests_properties(${name} PROPERTIES LABELS "runtime;noexceptions")
  endforeach()
endif()

# A negative compilation must fail for the intended diagnostic. Each case
# gets an independent target and log, so parallel CTest is safe.
function(jacquard_negative name source diagnostic)
  set(args)
  foreach(definition IN LISTS ARGN)
    list(APPEND args --define "${definition}")
  endforeach()
  if(MSVC)
    set(style msvc)
  else()
    set(style gnu)
  endif()
  add_test(NAME negative.${name}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/compile_negative.py"
      --compiler "${CMAKE_CXX_COMPILER}" --style "${style}"
      --source "${source}" --include "${PROJECT_SOURCE_DIR}/include"
      --include "${CMAKE_CURRENT_SOURCE_DIR}" --expect "${diagnostic}"
      --output "${CMAKE_CURRENT_BINARY_DIR}/compile-negative/${name}.log" ${args})
  set_tests_properties(negative.${name} PROPERTIES LABELS compile-negative TIMEOUT 60)
endfunction()

jacquard_negative(missing_hook "${CMAKE_CURRENT_SOURCE_DIR}/strict_missing.cpp" "trans_t<.*Failed.*Retry.*Cancelled")
jacquard_negative(missing_guard "${CMAKE_CURRENT_SOURCE_DIR}/strict_missing_guard.cpp" "guard_t<")
jacquard_negative(dead_branch "${CMAKE_CURRENT_SOURCE_DIR}/dead_branch.cpp" "non-final branch")
jacquard_negative(missing_payload "${CMAKE_CURRENT_SOURCE_DIR}/payload_missing.cpp" "missing declared payload")
jacquard_negative(wrong_typed_payload "${CMAKE_CURRENT_SOURCE_DIR}/payload_wrong.cpp" "payload type must match declared payload")
jacquard_negative(wrong_event_payload "${CMAKE_CURRENT_SOURCE_DIR}/payload_wrong.cpp" "payload type must match declared payload" JACQUARD_TEST_EVENT_OBJECT)
jacquard_negative(incomplete_rulebook "${CMAKE_CURRENT_SOURCE_DIR}/rulebook_missing.cpp" "trans_t<.*Connecting.*Connected.*Open")
jacquard_negative(void_hook_result "${CMAKE_CURRENT_SOURCE_DIR}/hook_wrong_result.cpp" "trans_t handler must return bool" JACQUARD_VOID_HOOK)
jacquard_negative(int_hook_result "${CMAKE_CURRENT_SOURCE_DIR}/hook_wrong_result.cpp" "trans_t handler must return bool")
jacquard_negative(void_hook_dispatch "${CMAKE_CURRENT_SOURCE_DIR}/hook_wrong_result.cpp" "trans_t handler must return bool" JACQUARD_VOID_HOOK JACQUARD_DISPATCH_ONLY)
jacquard_negative(unknown_terminal "${PROJECT_SOURCE_DIR}/tools/emit_tla.cpp" "UnknownTerminal" "SM_DEF=\"models/lifecycle.inc\"" JACQUARD_BAD_TERMINAL=UnknownTerminal)
jacquard_negative(count_terminal "${PROJECT_SOURCE_DIR}/tools/emit_tla.cpp" "TERMINAL must name a declared state" "SM_DEF=\"models/lifecycle.inc\"" JACQUARD_BAD_TERMINAL=COUNT)

function(lifecycle_emitter name terminals)
  jacquard_add_emitter(model_${name} "models/lifecycle.inc" ${ARGN})
  target_include_directories(model_${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME schema.${name}
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/tla.py" generate
      --emitter "$<TARGET_FILE:model_${name}>" --output "${CMAKE_CURRENT_BINARY_DIR}/schema/${name}"
      --terminals "${terminals}")
  set_tests_properties(schema.${name} PROPERTIES LABELS generator)
endfunction()

lifecycle_emitter(healthy "{\"Done\"}")
lifecycle_emitter(missing_completion "{\"Done\"}" JACQUARD_MISSING_COMPLETION)
lifecycle_emitter(selfloop_completion "{\"Done\"}" JACQUARD_SELFLOOP_COMPLETION)
lifecycle_emitter(allowed_terminal_selfloop "{\"Done\"}" JACQUARD_TERMINAL_SELFLOOP)
lifecycle_emitter(omitted_terminals "{}" JACQUARD_OMIT_TERMINALS)
lifecycle_emitter(early_completion "{\"Done\"}" JACQUARD_EARLY_COMPLETION)

jacquard_add_emitter(model_spin "selfloop.inc")
target_include_directories(model_spin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME schema.spin COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/tla.py" generate
  --emitter "$<TARGET_FILE:model_spin>" --output "${CMAKE_CURRENT_BINARY_DIR}/schema/spin" --terminals "{}")
set_tests_properties(schema.spin PROPERTIES LABELS generator)

jacquard_add_tla_test(NAME tla.healthy EMITTER model_healthy)
jacquard_add_tla_test(NAME tla.allowed_terminal_selfloop EMITTER model_allowed_terminal_selfloop)
foreach(name IN ITEMS spin missing_completion selfloop_completion omitted_terminals)
  jacquard_add_tla_test(NAME tla.${name} EMITTER model_${name} EXPECT_INVARIANT NoStuck)
endforeach()

# An upstream business invariant, independent of the generated NoStuck check.
jacquard_add_tla_test(NAME tla.business_safe EMITTER model_healthy
  MODULE models/LifecycleProperties.tla CONFIG models/Business.cfg)
jacquard_add_tla_test(NAME tla.early_completion_structural_safe EMITTER model_early_completion)
jacquard_add_tla_test(NAME tla.early_completion EMITTER model_early_completion
  MODULE models/LifecycleProperties.tla CONFIG models/Business.cfg EXPECT_INVARIANT DoneRequiresComplete)
jacquard_add_tla_test(NAME tla.done_witness EMITTER model_healthy
  MODULE models/LifecycleProperties.tla CONFIG models/Witness.cfg EXPECT_INVARIANT NeverDone KIND witness)
