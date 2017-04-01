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

#include "FindPostbranchCall.h"
#include "DataUtility.h"

// Record call-log pair
void FindPostbranchCallVisitor::recordCallLog(CallExpr *callExpr, CallExpr *logExpr){
    
    if(!callExpr || !logExpr)
        return;
    if(!callExpr->getDirectCallee() || !logExpr->getDirectCallee())
        return;
    
    FunctionDecl* callDecl = callExpr->getDirectCallee();
    FunctionDecl* logDecl = logExpr->getDirectCallee();

    string callName = callDecl->getNameAsString();
    string logName = logDecl->getNameAsString();
    
    FullSourceLoc callStart = CI->getASTContext().getFullLoc(callExpr->getLocStart());
    if(!callStart.isValid())
        return;
    
    callStart = callStart.getExpansionLoc();
    string callLoc = callStart.printToString(callStart.getManager());
    string callFile = callLoc.substr(0, callLoc.find_first_of(':'));
    SmallString<128> callFullPath(callFile);
    CI->getFileManager().makeAbsolutePath(callFullPath);
    
    // Store the call information into CallData
    CallData callData;
    callData.addPostbranchCall(callName, logName, callFullPath.str());
}

// Get the source code of given stmt
StringRef FindPostbranchCallVisitor::expr2str(Stmt *s){
    
    if(!s)
        return "";
    
    FullSourceLoc begin = CI->getASTContext().getFullLoc(s->getLocStart());
    FullSourceLoc end = CI->getASTContext().getFullLoc(s->getLocEnd());
    
    if(begin.isInvalid() || end.isInvalid())
        return "";
    
    SourceRange sr(begin.getExpansionLoc(), end.getExpansionLoc());
    
    return Lexer::getSourceText(CharSourceRange::getTokenRange(sr), CI->getSourceManager(), LangOptions(), 0);
}

// Search call site in given stmt and invoke the recordCallLog method
CallExpr* FindPostbranchCallVisitor::searchCall(Stmt *stmt, CallExpr* callExpr){
    
    if(!stmt)
        return 0;
    
    if(CallExpr *call = dyn_cast<CallExpr>(stmt)){
        if(callExpr != nullptr)
            recordCallLog(callExpr, call);
        else
            return call;
    }
    
    for(Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it){
            CallExpr *ret = searchCall(child, callExpr);
            if(ret)
                return ret;
        }
    }
    
    return 0;
}

// Trave the statement and find post-branch call, which is the potential log call
void FindPostbranchCallVisitor::travelStmt(Stmt *stmt, Stmt *father){
    
    if(!stmt || !father)
        return;

    // Deal with 1st log pattern
    //code
    //  if(foo())
    //      log();
    //\code
    
    // Find if(foo()), stmt is located in the condexpr of ifstmt
    if(IfStmt *ifStmt = dyn_cast<IfStmt>(father)){
        if(ifStmt->getCond() == stmt){
            // Search for call in if condexpr
            if(CallExpr* callExpr = searchCall(stmt, nullptr)){
                // Search for log in both 'then body' and 'else body'
                searchCall(ifStmt->getThen(), callExpr);
                searchCall(ifStmt->getElse(), callExpr);
            }
        }
    }
    
    // Deal with 2nd log pattern
    //code
    //  switch(foo());
    //      ...
    //      log();
    //\code
    
    // Find switch(foo()), stmt is located in the condexpr of switchstmt
    if(SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(father)){
        if(switchStmt->getCond() == stmt){
            // Search for call in switch condexpr
            if(CallExpr* callExpr = searchCall(stmt, nullptr)){
                // Search for log in switch body
                searchCall(switchStmt->getBody(), callExpr);
            }
        }
    }
    
    ///deal with 3rd log pattern
    //code
    //  ret = foo();
    //  if(ret)
    //      log();
    //\code
    
    ///find 'ret = foo()', when stmt = 'BinaryOperator', op == '=' and RHS == 'callexpr'
    if(BinaryOperator *binaryOperator = dyn_cast<BinaryOperator>(stmt)){
        if(binaryOperator->getOpcode() == BO_Assign){
            if(Expr *expr = binaryOperator->getRHS()){
                expr = expr->IgnoreImpCasts();
                expr = expr->IgnoreImplicit();
                expr = expr->IgnoreParens();
                if(CallExpr *callExpr = dyn_cast<CallExpr>(expr)){
                    
                    string returnName = expr2str(binaryOperator->getLHS());
                    
                    ///search for brothers of stmt
                    ///young_brother means the branch after stmt
                    bool isYoungBrother = false;
                    for(Stmt::child_iterator bro = father->child_begin(); bro != father->child_end(); ++bro){
                        
                        if(Stmt *brother = *bro){
                            
                            ///we only search for branches after stmt
                            if(brother == stmt){
                                isYoungBrother = true;
                                continue;
                            }
                            if(isYoungBrother == false){
                                continue;
                            }
                            
                            // Find 'if(ret)'
                            if(IfStmt *ifStmt = dyn_cast<IfStmt>(brother)){
                                // The return name, which is checked in condexpr
                                StringRef condString = expr2str(ifStmt->getCond());
                                if(returnName!= "" && condString.find(returnName) != string::npos){
                                    // Search for log in both 'then body' and 'else body'
                                    searchCall(ifStmt->getThen(), callExpr);
                                    searchCall(ifStmt->getElse(), callExpr);
                                }
                            }
                            // Find 'switch(ret)'
                            else if(SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(brother)){
                                // The return name, which is checked in condexpr
                                StringRef condString = expr2str(switchStmt->getCond());
                                if(returnName!= "" && condString.find(returnName) != string::npos){
                                    // Search for log in switch body
                                    searchCall(switchStmt->getBody(), callExpr);
                                }
                            }
                        }
                    }//end for
                }
            }
        }
    }//end if(BinaryOperator
    
    for(Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            travelStmt(child, stmt);
    }
    
    return;
}

// Visit the function declaration and travel the function body
bool FindPostbranchCallVisitor::VisitFunctionDecl (FunctionDecl* Declaration){
    
    if(!(Declaration->isThisDeclarationADefinition() && Declaration->hasBody()))
        return true;
    
    FullSourceLoc functionstart = CI->getASTContext().getFullLoc(Declaration->getLocStart()).getExpansionLoc();
    if(!functionstart.isValid())
        return true;
    if(functionstart.getFileID() != CI->getSourceManager().getMainFileID())
        return true;
    
    //errs()<<"Found function "<<Declaration->getQualifiedNameAsString() ;
    //errs()<<" @ " << functionstart.printToString(functionstart.getManager()) <<"\n";
    
    if(Stmt* function = Declaration->getBody()){
        travelStmt(function, function);
    }
    
    return true;
}

// Handle the translation unit and visit each function declaration
void FindPostbranchCallConsumer::HandleTranslationUnit(ASTContext& Context) {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    return;
}

// If the tool finds more than one entry in json file for a file, it just runs multiple times,
// once per entry. As far as the tool is concerned, two compilations of the same file can be
// entirely different due to differences in flags. However, we don't want to see these ruining
// our statistics. More detials see:
// http://eli.thegreenplace.net/2014/05/21/compilation-databases-for-clang-based-tools
map<string, bool> FindPostbranchCallAction::hasAnalyzed;

// Creat FindFunctionCallConsuer instance and return to ActionFactory
std::unique_ptr<clang::ASTConsumer> FindPostbranchCallAction::CreateASTConsumer(CompilerInstance& Compiler, StringRef InFile){
    if(hasAnalyzed[InFile] == 0){
        hasAnalyzed[InFile] = 1 ;
        return std::unique_ptr<ASTConsumer>(new FindPostbranchCallConsumer(&Compiler, InFile));
    }
    else{
        return nullptr;
    }
}
