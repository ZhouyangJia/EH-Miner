//===- DummyAction.h - A dummy frontend action outputting function calls-===//
//
//                     Cross Project Checking
//
// Author: Zhouyang Jia, PhD Candidate
// Affiliation: School of Computer Science, National University of Defense Technology
// Email: jiazhouyang@nudt.edu.cn
//
//===----------------------------------------------------------------------===//
//
// This file implements a dummy frontend action to output function calls.
//
//===----------------------------------------------------------------------===//


#ifndef DummyAction_h
#define DummyAction_h

#include "llvm/ADT/IntrusiveRefCntPtr.h"
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


class DummyVisitor : public RecursiveASTVisitor <DummyVisitor> {
public:
    explicit DummyVisitor(CompilerInstance* CI, StringRef InFile) : CI(CI), InFile(InFile){};

    // Trave the statement and find call expression
    bool VisitFunctionDecl (FunctionDecl*);

    // Visit the function declaration and travel the function body
    void travelStmt(Stmt*);
    
private:
    CompilerInstance* CI;
    StringRef InFile;
};


class DummyConsumer : public ASTConsumer {
public:
    explicit DummyConsumer(CompilerInstance* CI, StringRef InFile) : Visitor(CI, InFile){}
    
    // Handle the translation unit and visit each function declaration
    virtual void HandleTranslationUnit (clang::ASTContext &Context);
    
private:
    DummyVisitor Visitor;
    StringRef InFile;
};


class DummyAction : public ASTFrontendAction {
public:
    // Creat DummyConsuer instance and return to ActionFactory
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler, StringRef InFile);
    
private:
};

#endif /* DummyAction_h */
