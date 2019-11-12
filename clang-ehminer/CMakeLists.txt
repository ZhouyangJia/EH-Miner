include_directories("/opt/local/include")
link_directories("/opt/local/lib")

set( LLVM_LINK_COMPONENTS
${LLVM_TARGETS_TO_BUILD}
Option
Support
)

add_clang_executable(clang-ehminer
    src/FindBranchCall.cpp
    src/FindBranchCall.h
    src/DataUtility.cpp
    src/DataUtility.h
    src/Main.cpp
    )

target_link_libraries(clang-ehminer
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

install(TARGETS clang-ehminer
    RUNTIME DESTINATION bin)

#OPTION(SQLITE "Use SQLite to store function call infomation instead of memory." OFF)
#IF(SQLITE)
#    ADD_DEFINITIONS(-DSQLITE)
#ENDIF(SQLITE)
