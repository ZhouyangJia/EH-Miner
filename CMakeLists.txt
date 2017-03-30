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
  sqlite3
  )

install(TARGETS clang-dummy
  RUNTIME DESTINATION bin)

OPTION(SQLITE "Use SQLite to store function call infomation instead of memory, please enable this when analyzing tens of thouands of source files." OFF)
IF(SQLITE)
    ADD_DEFINITIONS(-DSQLITE)
ENDIF(SQLITE)
