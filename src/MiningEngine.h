//
//  MiningEngine.h
//  LLVM
//
//  Created by ZhouyangJia on 28/04/2017.
//
//

#ifndef MiningEngine_h
#define MiningEngine_h

#include <sqlite3.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <iostream>

using namespace std;

typedef pair<string, string> func;

struct CallSite{
    
    string domain;
    string project;
    
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
    
    void print();
};


class TargetFunction{
    
public:
    
    TargetFunction(func mFunc) : mFunc(mFunc) {}
    
    void addCallSite(CallSite mCallSite) {mCallSites.push_back(mCallSite);}
    
    void print();
    
private:
    func mFunc;
    vector<CallSite> mCallSites;
    
};


class MiningEngine{
    
public:
    MiningEngine(sqlite3* db, int min) : numMinProj(min) , db(db) {}
    
    void addTargetFunction(TargetFunction mTargetFunction) {mTargetFunctions.push_back(mTargetFunction);}
    
    void runAnalysis();
    
private:
    
    vector<func> funcVec;
    
    vector<TargetFunction> mTargetFunctions;
    
    int numMinProj;
    
    sqlite3* db;
};


#endif /* MiningEngine_h */
