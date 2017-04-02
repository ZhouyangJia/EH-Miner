//===- FindPostbranchCall.h - A frontend action outputting post-branch calls -===//
//
//                     Cross Project Checking
//
// Author: Zhouyang Jia, PhD Candidate
// Affiliation: School of Computer Science, National University of Defense Technology
// Email: jiazhouyang@nudt.edu.cn
//
//===----------------------------------------------------------------------===//
//
// This file implements a frontend action to output function calls which are
// called after checking by a branch statement.
//
//===----------------------------------------------------------------------===//

#ifndef FindPostbranchCall_h
#define FindPostbranchCall_h

#include <map>
#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "clang/Analysis/CFG.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/FrontendActions.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/StaticAnalyzer/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"

using namespace std;
using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;


//===----------------------------------------------------------------------===//
//
// FindPostbranchCallAction, FindPostbranchCallConsumer and FindPostbranchCallVisitor
//
//===----------------------------------------------------------------------===//
// These three classes work in tandem to prefrom the frontend analysis, if you
// have any question, please check the offical explanation:
//      http://clang.llvm.org/docs/RAVFrontendAction.html
//===----------------------------------------------------------------------===//

class FindPostbranchCallVisitor : public RecursiveASTVisitor <FindPostbranchCallVisitor> {
public:
    explicit FindPostbranchCallVisitor(CompilerInstance* CI, StringRef InFile) : CI(CI), InFile(InFile){};
    
    // Visit the function declaration and travel the function body
    bool VisitFunctionDecl (FunctionDecl* functionDecl);
    
    // Trave the statement and find post-brance call
    void travelStmt(Stmt* stmt, Stmt* father);

private:
    // Record call-log pair
    void recordCallLog(CallExpr *callExpr, CallExpr *logExpr);
        
    // Search pre-branch call site in given stmt
    CallExpr* searchPreBranchCall(Stmt* stmt);
    // Search post-branch call site in given stmt
    void searchPostBranchCall(Stmt* stmt, CallExpr* callExpr);
    // Search deeper-branch call site in given stmt
    void searchDeeperCall(Stmt* stmt, CallExpr* callExpr);
    
    // Get the source code of given stmt
    StringRef expr2str(Stmt* stmt);
    
    // Check whether the log has been recorded or not
    map<CallExpr*, bool> hasRecorded;
    
    CompilerInstance* CI;
    StringRef InFile;
    
};

class FindPostbranchCallConsumer : public ASTConsumer {
public:
    explicit FindPostbranchCallConsumer(CompilerInstance* CI, StringRef InFile) : Visitor(CI, InFile){}
    
    // Handle the translation unit and visit each function declaration
    virtual void HandleTranslationUnit (clang::ASTContext &Context);
    
private:
    FindPostbranchCallVisitor Visitor;
    StringRef InFile;
};

class FindPostbranchCallAction : public ASTFrontendAction {
public:
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile);
private:
    // If the tool finds more than one entry in json file for a file, it just runs multiple times,
    // once per entry. As far as the tool is concerned, two compilations of the same file can be
    // entirely different due to differences in flags. However, we don't want these to ruin our
    // statistics. More detials see:
    // http://eli.thegreenplace.net/2014/05/21/compilation-databases-for-clang-based-tools
    static map<string, bool> hasAnalyzed;
};

#endif /* FindPostbranchCall_h */
