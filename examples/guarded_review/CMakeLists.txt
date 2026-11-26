add_executable(guarded_review main.cpp)
target_link_libraries(guarded_review PRIVATE jacquard::jacquard)
jacquard_strict_cxx14(guarded_review)

if(BUILD_TESTING)
  add_test(NAME demo.guarded_review COMMAND guarded_review)
  set_tests_properties(demo.guarded_review PROPERTIES LABELS "runtime;demo")

  jacquard_add_tla_test(NAME tla.review_guards EMITTER emit_review
    MODULE ReviewProperties.tla CONFIG Safe.cfg)
  jacquard_add_tla_test(NAME tla.review_missing_guard EMITTER emit_review
    MODULE ReviewProperties.tla CONFIG MissingGuard.cfg EXPECT_INVARIANT ReviewedWithValidResult)
  jacquard_add_tla_test(NAME tla.review_hook_failure_commits EMITTER emit_review
    MODULE ReviewProperties.tla CONFIG HookFailureCommits.cfg EXPECT_INVARIANT ReviewedAfterHook)
  jacquard_add_tla_test(NAME tla.review_approved_witness EMITTER emit_review
    MODULE ReviewProperties.tla CONFIG ApprovedWitness.cfg EXPECT_INVARIANT NeverApproved KIND witness)
endif()
