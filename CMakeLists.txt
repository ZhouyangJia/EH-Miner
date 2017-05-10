include_directories("/usr/local/include")
link_directories("/usr/local/lib")

set( LLVM_LINK_COMPONENTS
${LLVM_TARGETS_TO_BUILD}
Option
Support
)

add_clang_executable(clang-dummy
    src/CallGraph.cpp
    src/CallGraph.h
    src/FindFunctionCall.cpp
    src/FindFunctionCall.h
    src/FindBranchCall.cpp
    src/FindBranchCall.h
    src/DataUtility.cpp
    src/DataUtility.h
    src/MiningEngine.cpp
    src/MiningEngine.h
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
    config
    sqlite3
    )

install(TARGETS clang-dummy
    RUNTIME DESTINATION bin)

#OPTION(SQLITE "Use SQLite to store function call infomation instead of memory." OFF)
#IF(SQLITE)
#    ADD_DEFINITIONS(-DSQLITE)
#ENDIF(SQLITE)
