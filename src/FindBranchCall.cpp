//===- FindBranchCall.h - A frontend action outputting pre/post-branch calls -===//
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

#include "FindBranchCall.h"
#include "DataUtility.h"

// Check whether the char belongs to a variable name or not
bool isVariableChar(char c){
    if(c >='0' && c<='9')
        return true;
    if(c >='a' && c<='z')
        return true;
    if(c >='A' && c<='Z')
        return true;
    if(c == '_')
        return true;
    
    return false;
}

// Get the source code of given stmt
StringRef FindBranchCallVisitor::expr2str(Stmt *s){
    
    if(!s)
        return "";
    
    FullSourceLoc begin = CI->getASTContext().getFullLoc(s->getLocStart());
    FullSourceLoc end = CI->getASTContext().getFullLoc(s->getLocEnd());
    
    if(begin.isInvalid() || end.isInvalid())
        return "";
    
    SourceRange sr(begin.getExpansionLoc(), end.getExpansionLoc());
    
    return Lexer::getSourceText(CharSourceRange::getTokenRange(sr), CI->getSourceManager(), LangOptions(), 0);
}

// Get the nodes from the expr of branch condition
vector<string> FindBranchCallVisitor::getExprNodeVec(Expr* expr){
    
    vector<string> ret;
    expr = expr->IgnoreImpCasts();
    expr = expr->IgnoreImplicit();
    expr = expr->IgnoreParens();
    
    if(auto *unaryOperator = dyn_cast<UnaryOperator>(expr)){
        vector<string> child = getExprNodeVec(unaryOperator->getSubExpr());
        
        ret.insert(ret.end(), child.begin(), child.end());
        ret.push_back(UnaryOperator::getOpcodeStr(unaryOperator->getOpcode()));
    }
    else if(auto *binaryOperator = dyn_cast<BinaryOperator>(expr)){
        vector<string> lchild = getExprNodeVec(binaryOperator->getLHS());
        vector<string> rchild = getExprNodeVec(binaryOperator->getRHS());
        
        ret.insert(ret.end(), lchild.begin(), lchild.end());
        ret.insert(ret.end(), rchild.begin(), rchild.end());
        ret.push_back(BinaryOperator::getOpcodeStr(binaryOperator->getOpcode()));
    }
    else if(auto *conditionalOperator = dyn_cast<ConditionalOperator>(expr)){
        vector<string> lchild = getExprNodeVec(conditionalOperator->getCond());
        vector<string> mchild = getExprNodeVec(conditionalOperator->getTrueExpr());
        vector<string> rchild = getExprNodeVec(conditionalOperator->getFalseExpr());
        
        ret.insert(ret.end(), lchild.begin(), lchild.end());
        ret.insert(ret.end(), mchild.begin(), mchild.end());
        ret.insert(ret.end(), rchild.begin(), rchild.end());
        ret.push_back(":?");
    }
    else{
        ret.push_back(expr2str(expr));
    }
    
    return ret;
}

// Record call-log/ret pair
void FindBranchCallVisitor::recordBranchCall(CallExpr *callExpr, CallExpr *logExpr, ReturnStmt* retStmt){
    
    if(!callExpr || (!logExpr && !retStmt))
        return;
    
    // Used to stroe the branch data
    CallData callData;
    
    // Collect callexpr information
    if(callExpr && !callExpr->getDirectCallee())
        return;
    
    FunctionDecl* callDecl = callExpr->getDirectCallee();
    string callName = callDecl->getNameAsString();
    
    FullSourceLoc callLoc = CI->getASTContext().getFullLoc(callExpr->getLocStart());
    FullSourceLoc callDef = CI->getASTContext().getFullLoc(callDecl->getLocStart());
    if(!callLoc.isValid() || !callDef.isValid())
        return;
    
    callLoc = callLoc.getExpansionLoc();
    callDef = callDef.getSpellingLoc();
    
    string callLocFile = callLoc.printToString(callLoc.getManager());
    string callDefFile = callDef.printToString(callDef.getManager());
    callDefFile = callDefFile.substr(0, callDefFile.find_first_of(':'));
    
    // The API callStart.printToString(callStart.getManager()) is behaving inconsistently,
    // more infomation see http://lists.llvm.org/pipermail/cfe-dev/2016-October/051092.html
    // So, we use makeAbsolutePath.
    SmallString<128> callLocFullPath(callLocFile);
    CI->getFileManager().makeAbsolutePath(callLocFullPath);
    SmallString<128> callDefFullPath(callDefFile);
    CI->getFileManager().makeAbsolutePath(callDefFullPath);
    
    // Collect branch condition information
    vector<string> exprNodeVec;
    string exprStr;
    if(auto *ifStmt = dyn_cast<IfStmt>(mBranchStmt)){
        exprNodeVec = getExprNodeVec(ifStmt->getCond());
        exprStr = expr2str(ifStmt->getCond());
    }
    else if(auto *switchStmt = dyn_cast<SwitchStmt>(mBranchStmt)){
        exprNodeVec = getExprNodeVec(switchStmt->getCond());
        exprStr = expr2str(switchStmt->getCond());
    }
    else{
        exprStr = "-";
    }
    
    // Arrange the branch info elements
    BranchInfo branchInfo;
    branchInfo.callName = callName;
    branchInfo.callDefLoc = callDefFullPath.str();
    branchInfo.callID = callLocFullPath.str();
    branchInfo.callStr = expr2str(callExpr);
    branchInfo.callReturn = mReturnName;
    for(unsigned i = 0; i < callExpr->getNumArgs(); i++){
        branchInfo.callArgVec.push_back(expr2str(callExpr->getArg(i)));
    }
    branchInfo.exprNodeVec = exprNodeVec;
    branchInfo.exprStr = exprStr;

    // Find a call-return pair
    if(retStmt != nullptr){
        
        // Collect retstmt information
        FullSourceLoc retLoc = CI->getASTContext().getFullLoc(retStmt->getLocStart());
        if(!retLoc.isValid()){
            return;
        }
        
        retLoc = retLoc.getExpansionLoc();
        string retLocFile = retLoc.printToString(retLoc.getManager());
        SmallString<128> retLocFullPath(retLocFile);
        CI->getFileManager().makeAbsolutePath(retLocFullPath);
        
        // Stroe the call-return info to callData
        branchInfo.logName = "return";
        branchInfo.logDefLoc = "-";
        branchInfo.logID = retLocFullPath.str();
        branchInfo.logStr = expr2str(retStmt);
        branchInfo.logArgVec.push_back(expr2str(retStmt->getRetValue()));
        callData.addBranchCall(branchInfo);
        
        // Store the post-branch and pre-branch info to callData
        callData.addPostbranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), "return", "-");
        callData.addPrebranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), "return", "-");
    }
    // Find a call-log pair
    else{
        
        // Collect logexpr information
        if(logExpr && !logExpr->getDirectCallee())
            return;
        
        FunctionDecl* logDecl = logExpr->getDirectCallee();
        string logName = logDecl->getNameAsString();
        
        FullSourceLoc logLoc = CI->getASTContext().getFullLoc(logExpr->getLocStart());
        FullSourceLoc logDef = CI->getASTContext().getFullLoc(logDecl->getLocStart());
        if(!logLoc.isValid() || !logDef.isValid())
            return;
        
        logLoc = logLoc.getExpansionLoc();
        logDef = logDef.getSpellingLoc();
        
        string logLocFile = logLoc.printToString(logLoc.getManager());
        string logDefFile = logDef.printToString(logDef.getManager());
        logDefFile = logDefFile.substr(0, logDefFile.find_first_of(':'));
        
        SmallString<128> logLocFullPath(logLocFile);
        CI->getFileManager().makeAbsolutePath(logLocFullPath);
        SmallString<128> logDefFullPath(logDefFile);
        CI->getFileManager().makeAbsolutePath(logDefFullPath);
        
        // Stroe the call-log info to callData
        branchInfo.logName = logName;
        branchInfo.logDefLoc = logDefFullPath.str();
        branchInfo.logID = logLocFullPath.str();
        branchInfo.logStr = expr2str(logExpr);
        for(unsigned i = 0; i < logExpr->getNumArgs(); i++){
            branchInfo.logArgVec.push_back(expr2str(logExpr->getArg(i)));
        }
        callData.addBranchCall(branchInfo);
        
        // Store the post-branch and pre-branch info to callData
        callData.addPostbranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), logName, logDefFullPath.str());
        pair<CallExpr*, string> mypair = make_pair(callExpr, logName);
        // For "if(foo()) bar(); bar();", ignore the second "bar()"
        if(hasSameLog[mypair] == false){
            hasSameLog[mypair] = true;
            callData.addPrebranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), logName, logDefFullPath.str());
        }
    }
    return;
}

// Search post-branch call site in given stmt and invoke the recordCallLog method
void FindBranchCallVisitor::searchPostBranchCall(Stmt *stmt, CallExpr* callExpr){
    
    if(!stmt || !callExpr)
        return;
    
    if(auto *call = dyn_cast<CallExpr>(stmt))
        recordBranchCall(callExpr, call, nullptr);
    
    if(auto *retstmt = dyn_cast<ReturnStmt>(stmt))
        recordBranchCall(callExpr, nullptr, retstmt);
    
    if(dyn_cast<IfStmt>(stmt) || dyn_cast<SwitchStmt>(stmt)){
        // So far, we only search for the first level
        return;
    }
    
    if(dyn_cast<WhileStmt>(stmt) || dyn_cast<ForStmt>(stmt))
        return;
    
    for(Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            searchPostBranchCall(child, callExpr);
    }
    
    return;
}

// Search pre-branch call site in given stmt and return the call
CallExpr* FindBranchCallVisitor::searchPreBranchCall(Stmt *stmt){
    
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
void FindBranchCallVisitor::travelStmt(Stmt *stmt, Stmt *father){
    
    if(!stmt || !father)
        return;
    
    // If find a call expression
    if(CallExpr* callExpr = dyn_cast<CallExpr>(stmt)){
        if(FunctionDecl* callFunctionDecl = callExpr->getDirectCallee()){
            
            // Get the name, call location and definition location of the call expression
            string callName = callFunctionDecl->getNameAsString();
            string funcName = FD->getNameAsString();
            FullSourceLoc callLoc = CI->getASTContext().getFullLoc(callExpr->getLocStart());
            FullSourceLoc callDef = CI->getASTContext().getFullLoc(callFunctionDecl->getLocStart());
            FullSourceLoc funcDef = CI->getASTContext().getFullLoc(FD->getLocStart());
            
            if(callLoc.isValid() && callDef.isValid() && funcDef.isValid()){
                
                callLoc = callLoc.getExpansionLoc();
                callDef = callDef.getSpellingLoc();
                funcDef = funcDef.getSpellingLoc();
                
                string callLocFile = callLoc.printToString(callLoc.getManager());
                string callDefFile = callDef.printToString(callDef.getManager());
                string funcDefFile = funcDef.printToString(funcDef.getManager());
                
                callLocFile = callLocFile.substr(0, callLocFile.find_first_of(':'));
                callDefFile = callDefFile.substr(0, callDefFile.find_first_of(':'));
                funcDefFile = funcDefFile.substr(0, funcDefFile.find_first_of(':'));
                
                // The API callStart.printToString(callStart.getManager()) is behaving inconsistently,
                // more infomation see http://lists.llvm.org/pipermail/cfe-dev/2016-October/051092.html
                // So, we use makeAbsolutePath.
                SmallString<128> callLocFullPath(callLocFile);
                CI->getFileManager().makeAbsolutePath(callLocFullPath);
                SmallString<128> callDefFullPath(callDefFile);
                CI->getFileManager().makeAbsolutePath(callDefFullPath);
                SmallString<128> funcDefFullPath(funcDefFile);
                CI->getFileManager().makeAbsolutePath(funcDefFullPath);
                
                // Store the call information into CallData
                CallData callData;
                callData.addFunctionCall(callName, callLocFullPath.str(), callDefFullPath.str());
                
                string callinfo = funcName + funcDefFullPath.str().str() + callName + callDefFullPath.str().str() + callLocFullPath.str().str();
                // Remove the duplicate edges in call graph
                if(hasRecorded[callinfo] == false){
                    hasRecorded[callinfo] = true;
                    callData.addCallGraph(funcName, funcDefFullPath.str(), callName, callDefFullPath.str(), callLocFullPath.str());
                }
            }
        }
    }

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
                mBranchStmt = father;
                mReturnName = "-";
                mPathNumber = 0;
                searchPostBranchCall(ifStmt->getThen(), callExpr);
                mPathNumber = 1;
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
                mBranchStmt = father;
                mReturnName = "-";
                mPathNumber = 0;
                for (SwitchCase *SC = switchStmt->getSwitchCaseList(); SC; SC = SC->getNextSwitchCase()){
                    searchPostBranchCall(SC->getSubStmt(), callExpr);
                    mPathNumber++;
                }
            }
        }
    }
    
    ///deal with 3rd log pattern
    //code
    //  ret = foo();    /   ret = foo();
    //  if(ret)         /   switch(ret)
    //      log();      /       case 1: log();
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
                    mReturnName = returnName;
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
                                    if(keyVariables[i] != ""){
                                        if(condString != keyVariables[i]){
                                            size_t pos = condString.find(keyVariables[i]);
                                            unsigned keylen = keyVariables[i].size();
                                            unsigned conlen = condString.size();
                                            if(pos == string::npos)
                                                continue;
                                            if(pos + keylen < conlen && isVariableChar(condString[pos+keylen]))
                                                continue;
                                            if(pos > 0 && isVariableChar(condString[pos-1]))
                                                continue;
                                        }
                                        
                                        // Search for log in both 'then body' and 'else body'
                                        mBranchStmt = brother;
                                        mPathNumber = 0;
                                        searchPostBranchCall(ifStmt->getThen(), callExpr);
                                        mPathNumber = 1;
                                        searchPostBranchCall(ifStmt->getElse(), callExpr);
                                        break;
                                    }
                                }
                            }
                            
                            // Find 'switch(ret)'
                            if(SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(brother)){
                                // The return name, which is checked in condexpr
                                StringRef condString = expr2str(switchStmt->getCond());
                                for(unsigned i = 0; i < keyVariables.size(); i++){
                                    if(keyVariables[i] != ""){
                                        if(condString != keyVariables[i]){
                                            size_t pos = condString.find(keyVariables[i]);
                                            unsigned keylen = keyVariables[i].size();
                                            unsigned conlen = condString.size();
                                            if(pos == string::npos)
                                                continue;
                                            if(pos + keylen < conlen && isVariableChar(condString[pos+keylen]))
                                                continue;
                                            if(pos > 0 && isVariableChar(condString[pos-1]))
                                                continue;
                                        }
                                        
                                        // Search for log in switch body
                                        mBranchStmt = brother;
                                        mPathNumber = 0;
                                        for (SwitchCase *SC = switchStmt->getSwitchCaseList(); SC; SC = SC->getNextSwitchCase()){
                                            searchPostBranchCall(SC->getSubStmt(), callExpr);
                                            mPathNumber++;
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }//end for
                }
            }
        }
    }//end if(BinaryOperator)
    
    for(Stmt::child_iterator it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            travelStmt(child, stmt);
    }
    
    return;
}

// Visit the function declaration and travel the function body
bool FindBranchCallVisitor::VisitFunctionDecl (FunctionDecl* Declaration){
    
    if(!(Declaration->isThisDeclarationADefinition() && Declaration->hasBody()))
        return true;
    
    FullSourceLoc functionstart = CI->getASTContext().getFullLoc(Declaration->getLocStart()).getExpansionLoc();
    if(!functionstart.isValid())
        return true;
    if(functionstart.getFileID() != CI->getSourceManager().getMainFileID())
        return true;
    
    //llvm::errs()<<"Found function "<<Declaration->getQualifiedNameAsString() ;
    //llvm::errs()<<" @ " << functionstart.printToString(functionstart.getManager()) <<"\n";
    
    
    FD = Declaration;
    
    hasRecorded.clear();
    hasSameLog.clear();
    if(Stmt* function = Declaration->getBody()){
        travelStmt(function, function);
    }
    
    return true;
}

// Handle the translation unit and visit each function declaration
void FindBranchCallConsumer::HandleTranslationUnit(ASTContext& Context) {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    return;
}

// If the tool finds more than one entry in json file for a file, it just runs multiple times,
// once per entry. As far as the tool is concerned, two compilations of the same file can be
// entirely different due to differences in flags. However, we don't want to see these ruining
// our statistics. More detials see:
// http://eli.thegreenplace.net/2014/05/21/compilation-databases-for-clang-based-tools
map<string, bool> FindBranchCallAction::hasAnalyzed;

// Creat FindFunctionCallConsuer instance and return to ActionFactory
std::unique_ptr<clang::ASTConsumer> FindBranchCallAction::CreateASTConsumer(CompilerInstance& Compiler, StringRef InFile){
    if(hasAnalyzed[InFile] == 0){
        hasAnalyzed[InFile] = 1 ;
        return std::unique_ptr<ASTConsumer>(new FindBranchCallConsumer(&Compiler, InFile));
    }
    else{
        return nullptr;
    }
}
