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
    
    if(hasRecorded[logExpr] != false){
        return;
    }
    hasRecorded[logExpr] = true;
    
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

// Search deeper-branch call site in given stmt and invoke the recordCallLog method
void FindPostbranchCallVisitor::searchDeeperCall(Stmt *stmt, CallExpr* callExpr){
    
    if(!stmt || !callExpr)
        return;
    
    if(CallExpr *call = dyn_cast<CallExpr>(stmt))
        recordCallLog(callExpr, call);
    
    // We don't search for the 3rd level of nested if stmt any more
    if(dyn_cast<IfStmt>(stmt) || dyn_cast<SwitchStmt>(stmt))
        return;
    
    if(dyn_cast<WhileStmt>(stmt) || dyn_cast<ForStmt>(stmt))
        return;
        
    for(Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            searchPostBranchCall(child, callExpr);
    }
    
    return;
}

// Search post-branch call site in given stmt and invoke the recordCallLog method
void FindPostbranchCallVisitor::searchPostBranchCall(Stmt *stmt, CallExpr* callExpr){
    
    if(!stmt || !callExpr)
        return;
    
    if(CallExpr *call = dyn_cast<CallExpr>(stmt))
            recordCallLog(callExpr, call);
    
    if(dyn_cast<IfStmt>(stmt) || dyn_cast<SwitchStmt>(stmt)){
        return;
        // Search for the 2nd level of nested if stmt
        //searchDeeperCall(stmt, callExpr);
    }
    
    if(dyn_cast<WhileStmt>(stmt) || dyn_cast<ForStmt>(stmt))
        return;
    
    for(Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            searchPostBranchCall(child, callExpr);
    }
    
    return;
}

// Search pre-branch call site in given stmt and invoke the recordCallLog method
CallExpr* FindPostbranchCallVisitor::searchPreBranchCall(Stmt *stmt){
    
    if(!stmt)
        return 0;
    
    if(CallExpr *call = dyn_cast<CallExpr>(stmt))
            return call;
    
    for(Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it){
            CallExpr *ret = searchPreBranchCall(child);
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
            if(CallExpr* callExpr = searchPreBranchCall(stmt)){
                // Search for log in both 'then body' and 'else body'
                searchPostBranchCall(ifStmt->getThen(), callExpr);
                searchPostBranchCall(ifStmt->getElse(), callExpr);
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
            if(CallExpr* callExpr = searchPreBranchCall(stmt)){
                // Search for log in switch body
                searchPostBranchCall(switchStmt->getBody(), callExpr);
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
                    
                    // Collect return value and arguments.
                    vector<string> keyVariables;
                    keyVariables.clear();
                    string returnName = expr2str(binaryOperator->getLHS());
                    keyVariables.push_back(returnName);
                    for(unsigned i = 0; i < callExpr->getNumArgs(); i++){
                        keyVariables.push_back(expr2str(callExpr->getArg(i)));
                    }
                    
                    ///search forwards for brothers of stmt
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
                            
                            // The non-reaching definition will be ignored
                            if(BinaryOperator *bo = dyn_cast<BinaryOperator>(brother)){
                                if(bo->getOpcode() == BO_Assign){
                                    string assignName = expr2str(bo->getLHS());
                                    for(vector<string>::iterator it=keyVariables.begin();it!=keyVariables.end();){
                                        if(assignName != "" && *it == assignName){
                                            it=keyVariables.erase(it);
                                        }
                                        else
                                            ++it;
                                    }
                                }
                                continue;
                            }
                            
                            // Find 'if(ret)'
                            if(IfStmt *ifStmt = dyn_cast<IfStmt>(brother)){
                                // The return name, which is checked in condexpr
                                StringRef condString = expr2str(ifStmt->getCond());
                                for(unsigned i = 0; i < keyVariables.size(); i++){
                                    if(keyVariables[i] != "" && (condString == keyVariables[i] ||
                                                            condString.find(keyVariables[i] + ".") == 0 ||
                                                            condString.find(keyVariables[i] + "[") == 0 ||
                                                            condString.find(keyVariables[i] + "->") == 0)){
                                        // Search for log in both 'then body' and 'else body'
                                        searchPostBranchCall(ifStmt->getThen(), callExpr);
                                        searchPostBranchCall(ifStmt->getElse(), callExpr);
                                    }
                                }
                            }
                            
                            // Find 'switch(ret)'
                            if(SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(brother)){
                                // The return name, which is checked in condexpr
                                StringRef condString = expr2str(switchStmt->getCond());
                                for(unsigned i = 0; i < keyVariables.size(); i++){
                                    if(keyVariables[i] != "" && (condString == keyVariables[i] ||
                                                                 condString.find(keyVariables[i] + ".") == 0 ||
                                                                 condString.find(keyVariables[i] + "[") == 0 ||
                                                                 condString.find(keyVariables[i] + "->") == 0)){
                                    // Search for log in switch body
                                    searchPostBranchCall(switchStmt->getBody(), callExpr);
                                    }
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
    
    hasRecorded.clear();
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
