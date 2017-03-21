set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

add_clang_executable(clang-dummy
  src/DummyAction.cpp
  src/Main.cpp
  )

target_link_libraries(clang-dummy
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  )

install(TARGETS clang-dummy
  RUNTIME DESTINATION bin)
