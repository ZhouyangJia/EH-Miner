set( LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Option
  Support
  )

include_directories("/usr/local/include")
link_directories("/usr/local/lib")

add_clang_executable(clang-dummy
  src/DummyAction.cpp
  src/DataUtility.cpp
  src/Main.cpp
  src/DummyAction.h
  src/DataUtility.h
  )

target_link_libraries(clang-dummy
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  config
  )

install(TARGETS clang-dummy
  RUNTIME DESTINATION bin)
