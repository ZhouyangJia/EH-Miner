//
//  MiningEngine.cpp
//  LLVM
//
//  Created by ZhouyangJia on 28/04/2017.
//
//

#include <stdio.h>
#include <iostream>

#include "MiningEngine.h"
#include "DataUtility.h"

using namespace std;

static int cb_target_functions(void *data, int argc, char **argv, char **azColName){

    pair<vector<func>*, int> * para =  (pair<vector<func>*, int>*) data;
    func myfunc(argv[0], argv[1]);
    int times = atoi(argv[2]);
    if(times >= para->second)
        para->first->push_back(myfunc);
    
    return SQLITE_OK;
}

static int cb_get_callsites(void *data, int argc, char **argv, char **azColName){

    TargetFunction * targetFunction = (TargetFunction *) data;
    
    CallSite callSite;
    callSite.domain = argv[1];
    callSite.project = argv[2];
    
    callSite.callID = argv[5];
    callSite.callStr = argv[6];
    callSite.callRet = argv[7];
    
    string callArgStr = argv[8];
    
    
    
    targetFunction->addCallSite(callSite);
    
    /*
     string callID;
     string callStr;
     string callRet;
     vector<string> callArgs;
     
     vector<string> exprNodes;
     vector<string> exprStrs;
     
     func log;
     string logID;
     string logStr;
     vector<string> logArgs;
     */
    
    return SQLITE_OK;
}

void CallSite::print(){
    cerr<<domain<<"#"<<project<<"#";
    cerr<<callID<<"#"<<callStr<<"#"<<callRet<<"#";
    cerr<<log.first<<"#"<<log.second<<"#"<<logID<<"#"<<logStr<<endl;
}

void TargetFunction::print(){
    
    cerr<<mFunc.first<<"@"<<mFunc.second<<endl;
    for(unsigned i = 0; i < mCallSites.size(); i++){
        mCallSites[i].print();
    }
}

void MiningEngine::runAnalysis(){
    
    if(db == nullptr){
        cerr<<"The database is null!"<<endl;
        exit(1);
    }
    
    string stmt;
    int rc;
    char *zErrMsg = 0;
    
    stmt = "select CallName, CallDefLoc, count(*) as times from call_statistic group by CallName, CallDefLoc";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    pair<vector<func>*, int> para(&funcVec, numMinProj);
    rc = sqlite3_exec(db, stmt.c_str(), cb_target_functions, &para, &zErrMsg);
    if(rc!=SQLITE_OK){
        cerr<<stmt<<endl;
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    for(unsigned i = 0; i < funcVec.size(); i++){
        TargetFunction targetFunction(funcVec[i]);
        
        stmt = "select * from branch_call where CallName = '" + funcVec[i].first + "' and CallDefLoc = '" + funcVec[i].second + "'";
        if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
        CallSite callSite;
        rc = sqlite3_exec(db, stmt.c_str(), cb_get_callsites, &targetFunction, &zErrMsg);
        if(rc!=SQLITE_OK){
            cerr<<stmt<<endl;
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        
        
        targetFunction.print();
    }

    return;
}
