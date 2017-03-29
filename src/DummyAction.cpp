//===- DummyAction.cpp - A dummy frontend action outputting function calls-===//
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


#include "DummyAction.h"
#include "DataUtility.h"

//===----------------------------------------------------------------------===//
//
//            DummyAction, DummyConsumer and DummyVisitor Classes
//
//===----------------------------------------------------------------------===//
// These three classes work in tandem to prefrom the frontend analysis, if you
// have any question, please check the offical explanation:
//      http://clang.llvm.org/docs/RAVFrontendAction.html
//===----------------------------------------------------------------------===//

// Trave the statement and find call expression
void DummyVisitor::travelStmt(Stmt* stmt){
    
    if(!stmt)
        return;
    
    // If find a call expression
    if(CallExpr* callExpr = dyn_cast<CallExpr>(stmt)){
        if(FunctionDecl* functionDecl = callExpr->getDirectCallee()){
            
            // Get the name, call location and definition location of the call expression
            string name = functionDecl->getNameAsString();
            FullSourceLoc callStart = CI->getASTContext().getFullLoc(callExpr->getLocStart());
            FullSourceLoc defStart = CI->getASTContext().getFullLoc(functionDecl->getLocStart());
            
            if(callStart.isValid() && defStart.isValid()){
                
                callStart = callStart.getExpansionLoc();
                defStart = defStart.getSpellingLoc();
                
                string callLoc = callStart.printToString(callStart.getManager());
                string defLoc = defStart.printToString(callStart.getManager());
                
                string callFile = callLoc.substr(0, callLoc.find_first_of(':'));
                string defFile = defLoc.substr(0, defLoc.find_first_of(':'));
                
                // The API callStart.printToString(callStart.getManager()) is behaving inconsistently,
                // more infomation see http://lists.llvm.org/pipermail/cfe-dev/2016-October/051092.html
                // So, we use makeAbsolutePath.
                SmallString<128> callFullPath(callFile);
                CI->getFileManager().makeAbsolutePath(callFullPath);
                SmallString<128> defFullPath(defFile);
                CI->getFileManager().makeAbsolutePath(defFullPath);
                
                // Store the call information into CallData
                CallData callData;
                callData.addCallExpression(name, callFullPath.str(), defFullPath.str());
            }
        }
    }
    
    // Travel the sub-statament
    for (Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            travelStmt(child);
    }
}

// Visit the function declaration and travel the function body
bool DummyVisitor::VisitFunctionDecl(FunctionDecl* Declaration) {
    if(!(Declaration->isThisDeclarationADefinition() && Declaration->hasBody()))
        return true;
    
    FullSourceLoc functionstart = CI->getASTContext().getFullLoc(Declaration->getLocStart()).getExpansionLoc();
    //FullSourceLoc functionend = CI->getASTContext().getFullLoc(Declaration->getLocEnd()).getExpansionLoc();
    
    if(!functionstart.isValid())
        return true;
    
    if(functionstart.getFileID() != CI->getSourceManager().getMainFileID())
        return true;
    
    //errs()<<"Found function "<<Declaration->getQualifiedNameAsString() ;
    //errs()<<" @ " << functionstart.printToString(functionstart.getManager()) <<"\n";
    
    if(Declaration->getBody()){
        travelStmt(Declaration->getBody());
    }
    return true;
}

// Handle the translation unit and visit each function declaration
void DummyConsumer::HandleTranslationUnit(ASTContext& Context) {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    return;
}

// Creat DummyConsuer instance and return to ActionFactory
std::unique_ptr<ASTConsumer> DummyAction::CreateASTConsumer(CompilerInstance& Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>(new DummyConsumer(&Compiler, InFile));
}
