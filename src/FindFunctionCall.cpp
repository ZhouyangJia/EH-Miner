//===-  FindFunctionCall.cpp - A frontend action outputting function calls-===//
//
//                     Cross Project Checking
//
// Author: Zhouyang Jia, PhD Candidate
// Affiliation: School of Computer Science, National University of Defense Technology
// Email: jiazhouyang@nudt.edu.cn
//
//===----------------------------------------------------------------------===//
//
// This file implements a frontend action to output function calls.
//
//===----------------------------------------------------------------------===//


#include "FindFunctionCall.h"
#include "DataUtility.h"

//===----------------------------------------------------------------------===//
//
// FindFunctionCallAction, FindFunctionCallConsumer and FindFunctionCallVisitor
//
//===----------------------------------------------------------------------===//
// These three classes work in tandem to prefrom the frontend analysis, if you
// have any question, please check the offical explanation:
//      http://clang.llvm.org/docs/RAVFrontendAction.html
//===----------------------------------------------------------------------===//

// Trave the statement and find function call
void FindFunctionCallVisitor::travelStmt(Stmt* stmt){
    
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
                callData.addFunctionCall(name, callFullPath.str(), defFullPath.str());
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
bool FindFunctionCallVisitor::VisitFunctionDecl(FunctionDecl* Declaration) {
    
    if(!(Declaration->isThisDeclarationADefinition() && Declaration->hasBody()))
        return true;
    
    FullSourceLoc functionstart = CI->getASTContext().getFullLoc(Declaration->getLocStart()).getExpansionLoc();
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
void FindFunctionCallConsumer::HandleTranslationUnit(ASTContext& Context) {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    return;
}

// If the tool finds more than one entry in json file for a file, it just runs multiple times,
// once per entry. As far as the tool is concerned, two compilations of the same file can be
// entirely different due to differences in flags. However, we don't want to see these ruining
// our statistics. More detials see:
// http://eli.thegreenplace.net/2014/05/21/compilation-databases-for-clang-based-tools
map<string, bool> FindFunctionCallAction::hasAnalyzed;

// Creat FindFunctionCallConsuer instance and return to ActionFactory
std::unique_ptr<ASTConsumer> FindFunctionCallAction::CreateASTConsumer(CompilerInstance& Compiler, StringRef InFile) {
    if(hasAnalyzed[InFile] == 0){
        hasAnalyzed[InFile] = 1 ;
        return std::unique_ptr<ASTConsumer>(new FindFunctionCallConsumer(&Compiler, InFile));
    }
    else{
        return nullptr;
    }
}
