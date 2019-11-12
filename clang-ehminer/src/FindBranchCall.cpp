//===- FindBranchCall.h - A frontend action outputting pre/post-branch calls -===//
//
//   EH-Miner: Mining Error-Handling Bugs without Error Specification Input
//
// Author: Zhouyang Jia, PhD Candidate
// Affiliation: School of Computer Science, National University of Defense Technology
// Email: jiazhouyang@nudt.edu.cn
//
//===----------------------------------------------------------------------===//
//
// This file implements a frontend action to emit information of function calls.
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
string FindBranchCallVisitor::getSourceCode(Stmt *stmt){
    
    if(!stmt)
        return "";
    
    std::string s;
    llvm::raw_string_ostream sos(s);
    PrintingPolicy pp(CI->getLangOpts());
    stmt->printPretty(sos, nullptr, pp);
    return sos.str();
}

// Get the nodes from the expr of branch condition
vector<string> FindBranchCallVisitor::getExprNodeVec(Expr* expr){
    
    vector<string> ret;
    expr = expr->IgnoreCasts();
    
    //expr->dump();
    // Three kinds of operators
    if(auto *parenExpr = dyn_cast<ParenExpr >(expr)){
        vector<string> child = getExprNodeVec(parenExpr->getSubExpr());
        
        ret.insert(ret.end(), child.begin(), child.end());
    }
    else if(auto *unaryOperator = dyn_cast<UnaryOperator>(expr)){
        vector<string> child = getExprNodeVec(unaryOperator->getSubExpr());
        
        ret.insert(ret.end(), child.begin(), child.end());
        string op = "UO";
        op += "_";
        char code[10];
        sprintf(code, "%d", unaryOperator->getOpcode());
        op += code;
        op += "_";
        op += UnaryOperator::getOpcodeStr(unaryOperator->getOpcode());
        ret.push_back(op);
        
        if(unaryOperator->getOpcodeStr(unaryOperator->getOpcode()) == "*"){
            const Type* type = unaryOperator->getSubExpr()->getType().getTypePtrOrNull();
            if(type == nullptr){
                cerr<<"Null type:"<<endl;
                unaryOperator->getSubExpr()->dump();
                ret.push_back("UO_VARIABLE_NULL");
            }
            else if(auto * pointerType = dyn_cast<PointerType>(type)){
                const Type* pointeeType = pointerType->getPointeeType().getTypePtrOrNull();
                if(pointeeType == nullptr){
                    cerr<<"Null pointee type of pointer type:"<<endl;
                    pointerType->dump();
                    ret.push_back("UO_VARIABLE_NULL");
                }
                else if(pointeeType->isIntegralOrEnumerationType() || pointeeType->isAnyCharacterType())
                    ret.push_back("UO_VARIABLE_INT");
                else if(pointeeType->isRealFloatingType())
                    ret.push_back("UO_VARIABLE_FLOAT");
                else if(pointeeType->isBooleanType())
                    ret.push_back("UO_VARIABLE_BOOL");
                else if(pointeeType->isAnyPointerType())
                    ret.push_back("UO_VARIABLE_POINTER");
                else
                    ret.push_back("UO_VARIABLE_NONE");
            }
            
        }
    }
    else if(auto *binaryOperator = dyn_cast<BinaryOperator>(expr)){
        vector<string> lchild = getExprNodeVec(binaryOperator->getLHS());
        vector<string> rchild = getExprNodeVec(binaryOperator->getRHS());
        
        ret.insert(ret.end(), lchild.begin(), lchild.end());
        ret.insert(ret.end(), rchild.begin(), rchild.end());
        
        string op = "BO";
        op += "_";
        char code[10];
        sprintf(code, "%d", binaryOperator->getOpcode());
        op += code;
        op += "_";
        op += BinaryOperator::getOpcodeStr(binaryOperator->getOpcode());
        ret.push_back(op);
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
    
    // All kinds of constants
    else if(auto *characterLiteral = dyn_cast<CharacterLiteral>(expr)){
        ostringstream oss;
        oss << characterLiteral->getValue();
        ret.push_back(oss.str());
        ret.push_back("UO_CONSTANT_INT");
    }
    else if(auto *floatingLiteral = dyn_cast<FloatingLiteral>(expr)){
        ostringstream oss;
        oss << floatingLiteral->getValueAsApproximateDouble();
        ret.push_back(oss.str());
        ret.push_back("UO_CONSTANT_FLOAT");
    }
    else if(auto *integerLiteral = dyn_cast<IntegerLiteral>(expr)){
        ret.push_back(integerLiteral->getValue().toString(10,true));
        ret.push_back("UO_CONSTANT_INT");
    }
    else if(auto *stringLiteral = dyn_cast<StringLiteral>(expr)){
        ret.push_back(stringLiteral->getString().str());
        ret.push_back("UO_CONSTANT_STRING");
    }
    else if(auto *boolLiteral = dyn_cast<CXXBoolLiteralExpr>(expr)){
        boolLiteral->getValue()?ret.push_back("1"):ret.push_back("0");
        ret.push_back("UO_CONSTANT_BOOL");
    }
    else if(dyn_cast<GNUNullExpr>(expr) || dyn_cast<CXXNullPtrLiteralExpr>(expr)){
        ret.push_back("0");
        ret.push_back("UO_CONSTANT_NULL");
    }
    
    // All kinds of variables (including array and member)
    else if(auto *declRefExpr = dyn_cast<DeclRefExpr>(expr)){
        if(auto *enumConstantDecl = dyn_cast<EnumConstantDecl>(declRefExpr->getFoundDecl())){
            ret.push_back(enumConstantDecl->getInitVal().toString(10,true));
            ret.push_back("UO_CONSTANT_INT");
        }
        else{
            ret.push_back(declRefExpr->getDecl()->getDeclName().getAsString());
            const Type* type = declRefExpr->getType().getTypePtrOrNull();
            
            if(type == nullptr){
                cerr<<"Null type:"<<endl;
                declRefExpr->dump();
                ret.push_back("UO_VARIABLE_NULL");
            }
            else if(type->isIntegralOrEnumerationType() || type->isAnyCharacterType())
                ret.push_back("UO_VARIABLE_INT");
            else if(type->isRealFloatingType())
                ret.push_back("UO_VARIABLE_FLOAT");
            else if(type->isBooleanType())
                ret.push_back("UO_VARIABLE_BOOL");
            else if(type->isAnyPointerType())
                ret.push_back("UO_VARIABLE_POINTER");
            else
                ret.push_back("UO_VARIABLE_NONE");
        }
    }
    else if(auto *arraySubscriptExpr = dyn_cast<ArraySubscriptExpr>(expr)){
        
        vector<string> base = getExprNodeVec(arraySubscriptExpr->getBase());
        vector<string> index = getExprNodeVec(arraySubscriptExpr->getIdx());
        
        ret.insert(ret.end(), base.begin(), base.end());
        ret.insert(ret.end(), index.begin(), index.end());
        
        ret.push_back("BO_ARRAY");
        
        const Type* type = arraySubscriptExpr->getType().getTypePtrOrNull();
        if(type == nullptr){
            cerr<<"Null type:"<<endl;
            declRefExpr->dump();
            ret.push_back("UO_VARIABLE_NULL");
        }
        else if(type->isIntegralOrEnumerationType() || type->isAnyCharacterType())
            ret.push_back("UO_VARIABLE_INT");
        else if(type->isRealFloatingType())
            ret.push_back("UO_VARIABLE_FLOAT");
        else if(type->isAnyCharacterType())
            ret.push_back("UO_VARIABLE_BOOL");
        else if(type->isAnyPointerType())
            ret.push_back("UO_VARIABLE_POINTER");
        else
            ret.push_back("UO_VARIABLE_NONE");
    }
    else if(auto *memberExpr = dyn_cast<MemberExpr>(expr)){
        ret.push_back(memberExpr->getMemberDecl()->getName().str());
        
        vector<string> base = getExprNodeVec(memberExpr->getBase());
        ret.insert(ret.end(), base.begin(), base.end());
        
        ret.push_back("BO_MEMBER");
        
        const Type* type = memberExpr->getType().getTypePtrOrNull();
        if(type == nullptr){
            cerr<<"Null type:"<<endl;
            declRefExpr->dump();
            ret.push_back("UO_VARIABLE_NULL");
        }
        else if(type->isIntegralOrEnumerationType() || type->isAnyCharacterType())
            ret.push_back("UO_VARIABLE_INT");
        else if(type->isRealFloatingType())
            ret.push_back("UO_VARIABLE_FLOAT");
        else if(type->isAnyCharacterType())
            ret.push_back("UO_VARIABLE_BOOL");
        else if(type->isAnyPointerType())
            ret.push_back("UO_VARIABLE_POINTER");
        else
            ret.push_back("UO_VARIABLE_NONE");
    }
    
    else if(auto *callExpr = dyn_cast<CallExpr>(expr)){
        ret.push_back(getSourceCode(expr));
        
        const Type* type = callExpr->getType().getTypePtrOrNull();
        if(type == nullptr){
            cerr<<"Null type:"<<endl;
            declRefExpr->dump();
            ret.push_back("UO_VARIABLE_NULL");
        }
        else if(type->isIntegralOrEnumerationType() || type->isAnyCharacterType())
            ret.push_back("UO_VARIABLE_INT");
        else if(type->isRealFloatingType())
            ret.push_back("UO_VARIABLE_FLOAT");
        else if(type->isBooleanType())
            ret.push_back("UO_VARIABLE_BOOL");
        else if(type->isAnyPointerType())
            ret.push_back("UO_VARIABLE_POINTER");
        else
            ret.push_back("UO_VARIABLE_NONE");
    }
    
    // Knonw expr, get source code directly
    else if(dyn_cast<StmtExpr>(expr) ||
            dyn_cast<UnaryExprOrTypeTraitExpr>(expr) ||
            dyn_cast<CXXUnresolvedConstructExpr>(expr) ||
            dyn_cast<CXXThisExpr>(expr) ||
            dyn_cast<CXXNewExpr>(expr) ||
            dyn_cast<OffsetOfExpr>(expr) ||
            dyn_cast<CompoundLiteralExpr>(expr) ||
            dyn_cast<ExprWithCleanups>(expr) ||
            dyn_cast<AtomicExpr>(expr) ){
        ret.push_back(getSourceCode(expr));
    }
    
    // Unknown expr, output expr type and get source code
    else{
        FullSourceLoc otherLoc;
        otherLoc = CI->getASTContext().getFullLoc(expr->getExprLoc());
        cerr<<"Unknown expr: "<<expr->getStmtClassName()<<" @ "<<otherLoc.getExpansionLoc().printToString(CI->getSourceManager())<<endl;
        ret.push_back(getSourceCode(expr));
    }
    
    return ret;
}

// Record call-log/ret pair
void FindBranchCallVisitor::recordBranchCall(CallExpr *callExpr, CallExpr *logExpr, ReturnStmt* retStmt, Stmt* otherStmt){
    
    if(!callExpr || (!logExpr && !retStmt && !otherStmt))
        return;
    
    // Used to stroe the branch data
    CallData callData;
    
    // Collect callexpr information
    if(callExpr && !callExpr->getDirectCallee())
        return;
    
    FunctionDecl* callDecl = callExpr->getDirectCallee();
    if(callDecl->getPreviousDecl())
        callDecl = callDecl->getPreviousDecl();
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
    vector<string> exprStr;
    vector<string> caseLabelStr;
    if(mBranchCondVec.size() != mPathNumberVec.size() || mPathNumberVec.size() != mSwitchCaseVec.size()){
        llvm::errs()<<"Something worng when analyzing condition expr!\n";
        return;
    }
    
    for(unsigned i = 0; i < mBranchCondVec.size(); i++){
        // Get the overall expr node vector
        vector<string> tmp = getExprNodeVec(mBranchCondVec[i]);
        exprNodeVec.insert(exprNodeVec.end(), tmp.begin(), tmp.end());
        if(mPathNumberVec[i] >= 10000){
            mPathNumberVec[i] -= 10000;
            exprNodeVec.push_back("UO_9_!");
        }
        if(mPathNumberVec[i] == 1)
            exprNodeVec.push_back("UO_9_!");
        caseLabelStr.push_back("-");
        
        if(mSwitchCaseVec[i] != nullptr){
            if(auto* caseStmt = dyn_cast<CaseStmt>(mSwitchCaseVec[i])){
                auto* mCaseLabel = caseStmt->getLHS();
            
                if(mCaseLabel){
                    tmp = getExprNodeVec(mCaseLabel);
                    exprNodeVec.insert(exprNodeVec.end(), tmp.begin(), tmp.end());
                    exprNodeVec.push_back("BO_13_==");
                    caseLabelStr.push_back(getSourceCode(mCaseLabel));
                }
                else{
                    exprNodeVec.push_back("__switch_default");
                    exprNodeVec.push_back("BO_13_==");
                    caseLabelStr.push_back("__switch_default");
                }
            }
        }
        
        // Record expr string vector
        exprStr.push_back(getSourceCode(mBranchCondVec[i]));
        
        // Connect two conditions from different branch statements
        if(i != 0)
            exprNodeVec.push_back("BO_18_&&");
    }
    
    // Arrange the branch info elements
    BranchInfo branchInfo;
    branchInfo.callName = callName;
    branchInfo.callDefLoc = callDefFullPath.str();
    branchInfo.callID = callLocFullPath.str();
    branchInfo.callStr = getSourceCode(callExpr);
    for(unsigned i = 0; i < mReturnNameVec.size(); i++){
        branchInfo.callReturnVec.push_back(mReturnNameVec[i]);
    }
    for(unsigned i = 0; i < callExpr->getNumArgs(); i++){
        branchInfo.callArgVec.push_back(getSourceCode(callExpr->getArg(i)));
    }
    branchInfo.exprNodeVec = exprNodeVec;
    branchInfo.exprStrVec = exprStr;
    branchInfo.caseLabelVec = caseLabelStr;
    branchInfo.pathNumberVec = mPathNumberVec;
    
    if(//branchInfo.callDefLoc.find("/usr") == string::npos ||
       branchInfo.callName.find("operator") != string::npos ||
       branchInfo.callName.find("__builtin") != string::npos)
        return;

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
        branchInfo.logStr = getSourceCode(retStmt);
        branchInfo.logArgVec.push_back(getSourceCode(retStmt->getRetValue()));
        branchInfo.logRetType = "-";
        branchInfo.logArgTypeVec.push_back("-");
        callData.addBranchCall(branchInfo);
        
        // Store the post-branch and pre-branch info to callData
        //callData.addPostbranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), "return", "-");
        //callData.addPrebranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), "return", "-");
    }
    // Find a call-continue/break/goto pair
    else if(otherStmt != nullptr){
        
        // Collect retstmt information
        FullSourceLoc loc = CI->getASTContext().getFullLoc(otherStmt->getLocStart());
        if(!loc.isValid()){
            return;
        }
        
        loc = loc.getExpansionLoc();
        string locFile = loc.printToString(loc.getManager());
        SmallString<128> locFullPath(locFile);
        CI->getFileManager().makeAbsolutePath(locFullPath);
        
        // Stroe the call-continue/break/goto info to callData
        string stmtstring = "";
        if(dyn_cast<ContinueStmt>(otherStmt))
            stmtstring = "continue";
        else if(dyn_cast<BreakStmt>(otherStmt))
            stmtstring = "break";
        else if(dyn_cast<GotoStmt>(otherStmt))
            stmtstring = "goto";
        else
            stmtstring = "unknown";
        branchInfo.logName = stmtstring;
        branchInfo.logDefLoc = "-";
        branchInfo.logID = locFullPath.str();
        branchInfo.logStr = getSourceCode(otherStmt);
        branchInfo.logRetType = "-";
        branchInfo.logArgTypeVec.push_back("-");
        callData.addBranchCall(branchInfo);
        
        // Store the post-branch and pre-branch info to callData
        //callData.addPostbranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), stmtstring, "-");
        //callData.addPrebranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), stmtstring, "-");
    }
    // Find a call-log pair
    else{
        
        // Collect logexpr information
        if(logExpr && !logExpr->getDirectCallee())
            return;
        
        FunctionDecl* logDecl = logExpr->getDirectCallee();
        if(logDecl->getPreviousDecl())
            logDecl = logDecl->getPreviousDecl();
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
        branchInfo.logStr = getSourceCode(logExpr);
        for(unsigned i = 0; i < logExpr->getNumArgs(); i++)
            branchInfo.logArgVec.push_back(getSourceCode(logExpr->getArg(i)));
        branchInfo.logRetType = logDecl->getReturnType().getAsString();
        for(unsigned i = 0; i < logDecl->getNumParams(); i++)
            branchInfo.logArgTypeVec.push_back(logDecl->getParamDecl(i)->getType().getAsString());
        
        callData.addBranchCall(branchInfo);
        
        // Store the post-branch and pre-branch info to callData
        //callData.addPostbranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), logName, logDefFullPath.str());
        //pair<CallExpr*, string> mypair = make_pair(callExpr, logName);
        // For "if(foo()) bar(); bar();", ignore the second "bar()"
        //if(hasSameLog[mypair] == false){
        //    hasSameLog[mypair] = true;
        //    callData.addPrebranchCall(callName, callLocFullPath.str(), callDefFullPath.str(), logName, logDefFullPath.str());
        //}
    }
    return;
}

// Search post-branch call site in given stmt and invoke the recordCallLog method
void FindBranchCallVisitor::searchPostBranchCall(Stmt *stmt, CallExpr* callExpr, string keyword, int deep){

    // generally speaking, there are not too many nested checks, like
    // ret = foo(); if(ret!=0)if(ret!=1)if(ret!=2)if(ret!=3)if(ret!=4)log();
    if(deep >= 5)
        return;
    
    if(!stmt || !callExpr)
        return;
    
    if(auto *call = dyn_cast<CallExpr>(stmt))
        recordBranchCall(callExpr, call, nullptr, nullptr);
    
    if(auto *retstmt = dyn_cast<ReturnStmt>(stmt))
        recordBranchCall(callExpr, nullptr, retstmt, nullptr);
    
    if(dyn_cast<ContinueStmt>(stmt) || dyn_cast<BreakStmt>(stmt) || dyn_cast<GotoStmt>(stmt))
        recordBranchCall(callExpr, nullptr, nullptr, stmt);

    // Search recursively for nested if and switch branch
    if(dyn_cast<IfStmt>(stmt) || dyn_cast<SwitchStmt>(stmt)){
        if(keyword != ""){
            vector<string> onekey;
            onekey.push_back(keyword);
            searchCheck(stmt, callExpr, onekey, deep + 1);
        }
        return;
    }
    
    // These two branches are few used to check
    if(dyn_cast<WhileStmt>(stmt) || dyn_cast<ForStmt>(stmt))
        return;
    
    // The non-reaching definition will be ignored, bug here
    if(auto *bo = dyn_cast<BinaryOperator>(stmt)){
        if(bo->getOpcode() == BO_Assign){
            string assignName = getSourceCode(bo->getLHS());
            if(keyword != "" && assignName == keyword)
                return;
        }
    }
    
    // Deal with flag
    //code
    //  if(foo())
    //    flag = 1;
    //  if(flag==1)
    //    log();
    //\code
    string flagname = "";
    string rightvalue = "";
    if(auto *bo = dyn_cast<BinaryOperator>(stmt)){
        if(bo->getOpcode() == BO_Assign){
            if(dyn_cast<IntegerLiteral>(bo->getRHS()->IgnoreCasts()) ||
               dyn_cast<CXXBoolLiteralExpr>(bo->getRHS()->IgnoreCasts())){
                flagname = getSourceCode(bo->getLHS()->IgnoreCasts());
                rightvalue = getSourceCode(bo->getRHS()->IgnoreCasts());
            }
            
            if(auto *declRefExpr = dyn_cast<DeclRefExpr>(bo->getRHS()->IgnoreCasts()))
                if(dyn_cast<EnumConstantDecl>(declRefExpr->getFoundDecl())){
                    flagname = getSourceCode(bo->getLHS()->IgnoreCasts());
                    rightvalue = getSourceCode(bo->getRHS()->IgnoreCasts());
                }
        }
    }
    string assignexpr = "";
    if(flagname != ""){
        assignexpr = getSourceCode(stmt);
        ParentMap PM(root);
        Stmt* me = stmt;
        Stmt* myfather = PM.getParent(me);
        int level = 0;
        while(me && myfather && me != myfather){
            level++;
            if(level == 10)
                break;
            if(!dyn_cast<IfStmt>(me) && !dyn_cast<SwitchStmt>(me)){
                me = myfather;
                myfather = PM.getParent(me);
                continue;
            }
            bool isYoungBrother = false;
            for(Stmt::child_iterator bro = myfather->child_begin(); bro != myfather->child_end(); ++bro){
                if(Stmt *brother = *bro){
                    
                    if(brother == me){
                        isYoungBrother = true;
                        continue;
                    }
                    if(isYoungBrother == false)
                        continue;
                    
                    // For IfStmt
                    if(IfStmt *ifStmt = dyn_cast<IfStmt>(brother)){
                        string condstr = getSourceCode(ifStmt->getCond());
                        if(condstr != flagname){
                            size_t pos = condstr.find(flagname);
                            unsigned keylen = flagname.size();
                            unsigned conlen = condstr.size();
                            if(pos == string::npos)
                                continue;
                            if(pos + keylen < conlen && isVariableChar(condstr[pos+keylen]))
                                continue;
                            if(pos > 0 && isVariableChar(condstr[pos-1]))
                                continue;
                        }
                        
                        //TODO: use z3 solver the detemine whether negate the condition or not
                        //llvm::errs()<<flagname<<"\n";
                        //llvm::errs()<<assignexpr<<"\n";
                        //llvm::errs()<<condstr<<"\n";
                        //assignexpr.replace(assignexpr.find("="), 2, "==");
                        /*
                        z3::context con;
                        z3::expr flag      = con.int_const(flagname.c_str());
                        z3::expr assign = con.constant(assignexpr.c_str(), con.bool_sort());
                        z3::expr cond = con.constant(condstr.c_str(), con.bool_sort());
                        z3::expr imply = assign && !cond;
                        z3::expr imply2 = (flag == 1) && !(flag > 0);
                        z3::solver sol(con);
                        sol.add(imply);
                        std::cout << sol;
                        switch (sol.check()) {
                            case z3::unsat:   std::cout << "unsat\n"; break;
                            case z3::sat:     std::cout << "sat\n"; break;
                            case z3::unknown: std::cout << "unknown\n"; break;
                        }*/
                        
                        /*
                        z3::context c;
                        z3::expr x = c.bool_const("x");
                        z3::expr y = c.bool_const("y");
                        z3::expr conjecture = (!x || !y);
                        
                        z3::solver s(c);
                        // adding the negation of the conjecture as a constraint.
                        s.add(!conjecture);
                        std::cout << s;
                        switch (s.check()) {
                            case z3::unsat:   std::cout << "de-Morgan is valid\n"; break;
                            case z3::sat:     std::cout << "de-Morgan is not valid\n"; break;
                            case z3::unknown: std::cout << "unknown\n"; break;
                        }*/
                        
                        searchPostBranchCall(ifStmt->getThen(), callExpr, "", 4);
                        searchPostBranchCall(ifStmt->getElse(), callExpr, "", 4);
                    }
                    
                    // For SwitchStmt
                    if(SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(brother)){
                        string condstr = getSourceCode(switchStmt->getCond());
                        if(condstr != flagname){
                            size_t pos = condstr.find(flagname);
                            unsigned keylen = flagname.size();
                            unsigned conlen = condstr.size();
                            if(pos == string::npos)
                                continue;
                            if(pos + keylen < conlen && isVariableChar(condstr[pos+keylen]))
                                continue;
                            if(pos > 0 && isVariableChar(condstr[pos-1]))
                                continue;
                        }
                        for (SwitchCase *SC = switchStmt->getSwitchCaseList(); SC; SC = SC->getNextSwitchCase()){
                            if(auto* caseStmt = dyn_cast<CaseStmt>(SC)){
                                auto* mCaseLabel = caseStmt->getLHS();
                                string caselabel = getSourceCode(mCaseLabel);
                                
                                // we will further negate the condition when pathnum >= 10000
                                if(caselabel != rightvalue){
                                    int pathnumber = mPathNumberVec[mPathNumberVec.size() - 1];
                                    mPathNumberVec.pop_back();
                                    mPathNumberVec.push_back(pathnumber + 10000);
                                }
                                searchPostBranchCall(SC->getSubStmt(), callExpr, "", 4);
                            }
                        }
                    }
                }
            }
            me = myfather;
            myfather = PM.getParent(me);
        }
    }
    
    for(auto it = stmt->child_begin(); it != stmt->child_end(); ++it){
        if(Stmt *child = *it)
            searchPostBranchCall(child, callExpr, keyword, deep);
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

// Search sub-condition in if-statement
// for if(a||b&&c), get if(a) if(b) if(c)
vector<Expr*> FindBranchCallVisitor::getSubCondition(Expr *expr){
    
    vector<Expr*> ret;
    
    if(!expr)
        return ret;
    
    if(auto *binaryOperator = dyn_cast<BinaryOperator>(expr)){
        if(binaryOperator->getOpcode() == BO_LAnd || binaryOperator->getOpcode() == BO_LOr){
            vector<Expr*> lchild = getSubCondition(binaryOperator->getLHS());
            vector<Expr*> rchild = getSubCondition(binaryOperator->getRHS());
            ret.insert(ret.end(), lchild.begin(), lchild.end());
            ret.insert(ret.end(), rchild.begin(), rchild.end());
            return ret;
        }
    }

    ret.push_back(expr);
    
    return ret;
}

// Search check candidate, we only care about if and switch
// input: branch statement, function call (if any), key variables (if any), deep of check condition
void FindBranchCallVisitor::searchCheck(Stmt* stmt, CallExpr* callExpr, vector<string> keyVariables, int deep){

    if(deep >= 5)
        return;
    
    if(IfStmt *ifStmt = dyn_cast<IfStmt>(stmt)){
        vector<Expr*> subcond = getSubCondition(ifStmt->getCond());
        for(unsigned i = 0; i < subcond.size(); i++){
            if(!callExpr){
                if(CallExpr* call = searchPreBranchCall(subcond[i])){
                    // Search for log in both 'then body' and 'else body'
                    mReturnNameVec.clear();
                    mReturnNameVec.push_back("-");
                    mBranchCondVec.push_back(subcond[i]);
                    mPathNumberVec.push_back(0);
                    mSwitchCaseVec.push_back(nullptr);
                    searchPostBranchCall(ifStmt->getThen(), call, "", deep);
                    mPathNumberVec.pop_back();
                    mPathNumberVec.push_back(1);
                    searchPostBranchCall(ifStmt->getElse(), call, "", deep);
                    mBranchCondVec.pop_back();
                    mPathNumberVec.pop_back();
                    mSwitchCaseVec.pop_back();
                }
                continue;
            }
            
            string condString = getSourceCode(subcond[i]);
            // check whether control dependent
            for(unsigned j = 0; j < keyVariables.size(); j++){
                if(keyVariables[j] == "")
                    continue;
                
                if(condString != keyVariables[j]){
                    size_t pos = condString.find(keyVariables[j]);
                    unsigned keylen = keyVariables[j].size();
                    unsigned conlen = condString.size();
                    if(pos == string::npos)
                        continue;
                    if(pos + keylen < conlen && isVariableChar(condString[pos+keylen]))
                        continue;
                    if(pos > 0 && isVariableChar(condString[pos-1]))
                        continue;
                }
                
                // Search for log in both 'then body' and 'else body'
                mBranchCondVec.push_back(subcond[i]);
                mPathNumberVec.push_back(0);
                mSwitchCaseVec.push_back(nullptr);
                searchPostBranchCall(ifStmt->getThen(), callExpr, keyVariables[j], deep);
                mPathNumberVec.pop_back();
                mPathNumberVec.push_back(1);
                searchPostBranchCall(ifStmt->getElse(), callExpr, keyVariables[j], deep);
                mBranchCondVec.pop_back();
                mPathNumberVec.pop_back();
                mSwitchCaseVec.pop_back();
            }
        }
    }
    
    if(SwitchStmt *switchStmt = dyn_cast<SwitchStmt>(stmt)){
        
        if(!callExpr){
            if(CallExpr* call = searchPreBranchCall(stmt)){
                // Search for log in switch body
                mReturnNameVec.clear();
                mReturnNameVec.push_back("-");
                int pathnum = 0;
                for (SwitchCase *SC = switchStmt->getSwitchCaseList(); SC; SC = SC->getNextSwitchCase()){
                    mBranchCondVec.push_back(switchStmt->getCond());
                    mPathNumberVec.push_back(pathnum);
                    mSwitchCaseVec.push_back(SC);
                    searchPostBranchCall(SC->getSubStmt(), call, "", deep);
                    mBranchCondVec.pop_back();
                    mPathNumberVec.pop_back();
                    mSwitchCaseVec.pop_back();
                    pathnum++;
                }
            }
        }
        
        string condString = getSourceCode(switchStmt->getCond());
        // check whether control dependent
        for(unsigned i = 0; i < keyVariables.size(); i++){
            if(keyVariables[i] == "")
                continue;
            
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
            int pathnum = 0;
            for (SwitchCase *SC = switchStmt->getSwitchCaseList(); SC; SC = SC->getNextSwitchCase()){
                mBranchCondVec.push_back(switchStmt->getCond());
                mPathNumberVec.push_back(pathnum);
                mSwitchCaseVec.push_back(SC);
                searchPostBranchCall(SC->getSubStmt(), callExpr, keyVariables[i], deep);
                mBranchCondVec.pop_back();
                mPathNumberVec.pop_back();
                mSwitchCaseVec.pop_back();
                pathnum++;
            }
        }
    }
}

// Trave the statement and find post-branch call, which is the potential log call
void FindBranchCallVisitor::travelStmt(Stmt *stmt, Stmt *father){
    
    if(!stmt || !father)
        return;
    
    fatherStmt[stmt] = father;
    
    // For callexpr, get statistics.
    if(CallExpr* callExpr = dyn_cast<CallExpr>(stmt)){
        if(FunctionDecl* callFunctionDecl = callExpr->getDirectCallee()){
            
            // For foo() in cat(){}, get its decl in foo.h
            //  # cat foo.h
            //  void foo();
            //  # cat foo.c
            //  void bar(){foo();}
            //  void foo(){}
            //  void cat(){foo();}
            if(callFunctionDecl->getPreviousDecl())
                callFunctionDecl = callFunctionDecl->getPreviousDecl();
            
            // Get the size of function body
            FullSourceLoc funcStart = CI->getASTContext().getFullLoc(FD->getLocStart());
            FullSourceLoc funcEnd = CI->getASTContext().getFullLoc(FD->getLocEnd());
            unsigned funcSize = funcEnd.getExpansionLineNumber() - funcStart.getExpansionLineNumber();
            
            FunctionDecl* functionDecl = FD;
            if(functionDecl->getPreviousDecl())
                functionDecl = functionDecl->getPreviousDecl();
            
            // Get the name, call location and definition location of the call expression
            string callName = callFunctionDecl->getNameAsString();
            string funcName = functionDecl->getNameAsString();
            FullSourceLoc callLoc = CI->getASTContext().getFullLoc(callExpr->getLocStart());
            FullSourceLoc callDef = CI->getASTContext().getFullLoc(callFunctionDecl->getLocStart());
            FullSourceLoc funcDef = CI->getASTContext().getFullLoc(functionDecl->getLocStart());
            
            if(callLoc.isValid() && callDef.isValid() && funcDef.isValid()){
                
                callLoc = callLoc.getExpansionLoc();
                callDef = callDef.getSpellingLoc();
                funcDef = funcDef.getSpellingLoc();
                
                string callLocFile = callLoc.printToString(callLoc.getManager());
                string callDefFile = callDef.printToString(callDef.getManager());
                string funcDefFile = funcDef.printToString(funcDef.getManager());
                
                string callStr = getSourceCode(callExpr);
                
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
                
                if(//callDefFullPath.str().find("/usr") != string::npos &&
                   callName.find("operator") == string::npos &&
                   callName.find("__builtin") == string::npos)
                    callData.addFunctionCall(callName, callLocFullPath.str(), callDefFullPath.str(), callStr);
                
                string callinfo = funcName + funcDefFullPath.str().str() + callName + callDefFullPath.str().str();
                // Remove the duplicate edges in call graph
                if(hasRecorded[callinfo] == false){
                    hasRecorded[callinfo] = true;
                    callData.addCallGraph(funcName, funcDefFullPath.str(), callName, callDefFullPath.str(), callLocFullPath.str(), funcSize);
                }
            }
        }
    }
    
    mBranchCondVec.clear();
    mPathNumberVec.clear();
    mSwitchCaseVec.clear();
    
    // Deal with 1st log pattern
    //code
    //  if(foo())
    //      log();
    //\code
    
    // Find if(foo()), stmt is located in the condexpr of ifstmt
    if(IfStmt *ifStmt = dyn_cast<IfStmt>(father)){
        if(ifStmt->getCond() == stmt){
            vector<string> nokeys;
            searchCheck(father, nullptr, nokeys, 0);
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
            vector<string> nokeys;
            searchCheck(father, nullptr, nokeys, 0);
        }
    }
    
    // Deal with 3rd log pattern
    //code
    //  ret = foo();    /   int ret = foo();
    //  if(ret)         /   switch(ret)
    //      log();      /       case 1: log();
    //\code

    CallExpr* callExpr = nullptr;
    string returnName = "";
    
    // Find 'int ret = foo()', when stmt = 'DeclStmt' and DeclInit == 'callexpr'
    if(DeclStmt *declStmt = dyn_cast<DeclStmt>(stmt)){
        if(declStmt->isSingleDecl() && declStmt->getSingleDecl()){
            if(VarDecl *valDecl = dyn_cast<VarDecl>(declStmt->getSingleDecl())){
                if(valDecl->getInit() && dyn_cast<CallExpr>(valDecl->getInit()->IgnoreCasts())){
                    returnName = valDecl->getName();
                    callExpr = dyn_cast<CallExpr>(valDecl->getInit()->IgnoreCasts());
                }
            }
        }
    }
    
    // Find 'ret = foo()', when stmt = 'BinaryOperator', op == '=' and RHS == 'callexpr'
    if(BinaryOperator *binaryOperator = dyn_cast<BinaryOperator>(stmt)){
        if(binaryOperator->getOpcode() == BO_Assign){
            if(Expr *expr = binaryOperator->getRHS()){
                if(dyn_cast<CallExpr>(expr->IgnoreCasts())){
                    returnName = getSourceCode(binaryOperator->getLHS());
                    callExpr = dyn_cast<CallExpr>(expr->IgnoreCasts());
                }
            }
        }
    }
    
    // Found 'ret = foo();' OR 'int ret = foo();'
    if(callExpr != nullptr && returnName != ""){
        
        // Collect arguments of callexpr
        vector<string> keyVariables;
        keyVariables.clear();
        mReturnNameVec.clear();
        
        while(returnName.size() > 0 && (returnName[0] == '&' || returnName[0] == '*'))
            returnName = returnName.substr(1, string::npos);
        mReturnNameVec.push_back(returnName);
        keyVariables.push_back(returnName);
        for(unsigned i = 0; i < callExpr->getNumArgs(); i++){
            string argstr = getSourceCode(callExpr->getArg(i));
            // Remove the & and *, e.g., &error -> error
            while(argstr.size() > 0 && (argstr[0] == '&' || argstr[0] == '*'))
                argstr = argstr.substr(1, string::npos);
            keyVariables.push_back(argstr);
        }
        
        // Deal with the path union situation
        //code
        //  if(a)
        //    ret == foo();
        //  else
        //    ret == foo();
        //  if(ret)
        //    log();
        //\code
        Stmt* me = stmt;
        Stmt* myfather = fatherStmt[me];
        int level = 0;
        while(me && myfather && me != myfather){
            level++;
            if(level == 10)
                break;
            
            bool dataflow = 0;
            // Deal with 'a=b=foo()', here, 'me' is the top BO (left '=').
            if(BinaryOperator *binaryOperator = dyn_cast<BinaryOperator>(me)){
                if(binaryOperator->getOpcode() == BO_Assign){
                    if(Expr *expr = binaryOperator->getRHS()){
                        if(expr == stmt){
                            dataflow = 1;
                            string newname = getSourceCode(binaryOperator->getLHS());
                            while(newname.size() > 0 && (newname[0] == '&' || newname[0] == '*'))
                                newname = newname.substr(1, string::npos);
                            mReturnNameVec.push_back(newname);
                            keyVariables.push_back(newname);
                        }
                    }
                }
            }
            // Deal with 'int a=b=foo()', here, 'me' is the top BO (left '=').
            if(DeclStmt *declStmt = dyn_cast<DeclStmt>(me)){
                if(declStmt->isSingleDecl() && declStmt->getSingleDecl()){
                    if(VarDecl *valDecl = dyn_cast<VarDecl>(declStmt->getSingleDecl())){
                        if(valDecl->getInit() == stmt){
                            dataflow = 1;
                            string newname = valDecl->getName();
                            while(newname.size() > 0 && (newname[0] == '&' || newname[0] == '*'))
                                newname = newname.substr(1, string::npos);
                            mReturnNameVec.push_back(newname);
                            keyVariables.push_back(newname);
                        }
                    }
                }
            }
            
            // In any case, we don't want to skip the current level
            if(me != stmt &&
               // For 'a=b=foo()' OR 'int a=b=foo()', we need to analyze its brothers.
               dataflow == 0 &&
               // For upper levels, we only analyze if and switch stmt
               !dyn_cast<IfStmt>(me) && !dyn_cast<SwitchStmt>(me)){
                
                me = myfather;
                myfather = fatherStmt[me];
                continue;
            }
            
            // Search forwards for brothers of stmt, young_brother means the branch after stmt
            bool isYoungBrother = false;
            for(Stmt::child_iterator bro = myfather->child_begin(); bro != myfather->child_end(); ++bro){
                if(Stmt *brother = *bro){
                    
                    // We only search for branches after stmt
                    if(brother == me){
                        isYoungBrother = true;
                        continue;
                    }
                    if(isYoungBrother == false)
                        continue;
                    
                    // For '='
                    if(BinaryOperator *bo = dyn_cast<BinaryOperator>(brother)){
                        if(bo->getOpcode() == BO_Assign){
                            // The non-reaching definition will be ignored: a=foo(); a=b;
                            string leftname = getSourceCode(bo->getLHS());
                            while(leftname.size() > 0 && (leftname[0] == '&' || leftname[0] == '*'))
                                leftname = leftname.substr(1, string::npos);
                            for(vector<string>::iterator it=keyVariables.begin();it!=keyVariables.end();){
                                if(leftname != "" && *it == leftname)
                                    it=keyVariables.erase(it);
                                else
                                    ++it;
                            }
                            for(vector<string>::iterator it=mReturnNameVec.begin();it!=mReturnNameVec.end();){
                                if(leftname != "" && *it == leftname)
                                    it=mReturnNameVec.erase(it);
                                else
                                    ++it;
                            }
                            
                            // Deal with the simple dataflow: a=foo(); b=a;
                            string rightname = getSourceCode(bo->getRHS());
                            while(rightname.size() > 0 && (rightname[0] == '&' || rightname[0] == '*'))
                                rightname = rightname.substr(1, string::npos);
                            for(unsigned i = 0; i < mReturnNameVec.size(); i++){
                                if(mReturnNameVec[i] == rightname){
                                    mReturnNameVec.push_back(leftname);
                                    keyVariables.push_back(leftname);
                                }
                            }
                        }
                        continue;
                    }
                    
                    // For DoStmt (pretty common in macro), search its children
                    if(DoStmt *doStmt = dyn_cast<DoStmt>(brother)){
                        if(CompoundStmt* compoundStmt = dyn_cast<CompoundStmt>(doStmt->getBody())){
                            for(Stmt::child_iterator child = compoundStmt->child_begin(); child != compoundStmt->child_end(); ++child){
                                Stmt *mychild = *child;
                                searchCheck(mychild, callExpr, keyVariables, 0);
                            }
                        }
                    }
                    
                    // For IfStmt and SwitchStmt
                    searchCheck(brother, callExpr, keyVariables, 0);
                }
            }//end for
            
            me = myfather;
            myfather = fatherStmt[me];
        }
    }
    
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
        root = function;
        fatherStmt.clear();
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
