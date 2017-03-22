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


void DummyVisitor::travelStmt(Stmt* stmt){
    
    if(!stmt)
        return;
    
    if(CallExpr* callExpr = dyn_cast<CallExpr>(stmt)){
        if(FunctionDecl* functionDecl = callExpr->getDirectCallee()){
            
            string name = functionDecl->getNameAsString();
            llvm::outs()<<name<<"@"<<InFile<<"\n";
        }
    }
    
    for (Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            travelStmt(child);
    }
}


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


void DummyConsumer::HandleTranslationUnit(ASTContext& Context) {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    return;
}


std::unique_ptr<ASTConsumer> DummyAction::CreateASTConsumer(CompilerInstance& Compiler, StringRef InFile) {
    return std::unique_ptr<ASTConsumer>(new DummyConsumer(&Compiler, InFile));
}
