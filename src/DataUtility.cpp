//===------ DataUtility.cpp - A utility class used for storing data   ----===//
//
//                     Cross Project Checking
//
// Author: Zhouyang Jia, PhD Candidate
// Affiliation: School of Computer Science, National University of Defense Technology
// Email: jiazhouyang@nudt.edu.cn
//
//===----------------------------------------------------------------------===//
//
// This file implements a utility class used for storing data.
//
//===----------------------------------------------------------------------===//

#include "DataUtility.h"

//===----------------------------------------------------------------------===//
//
//                     ConfigData Class
//
//===----------------------------------------------------------------------===//
// This class stores the data got from config file, including the domain
// information and projects in each domain. We use two static members to
// store the information so that it can be used all around our project.
//===----------------------------------------------------------------------===//

// The memebers are static, and used to store the domain and project name in config file
vector<string> ConfigData::domainName;
vector<vector<string>> ConfigData::projectName;

// Add a domain name from config file
void ConfigData::addDomainName(string name){
    domainName.push_back(name);
    vector<string> names;
    projectName.push_back(names);
}

// Add a project in domain numbered domainIndex
void ConfigData::addProjectName(unsigned int domainIndex, string name){
    projectName[domainIndex].push_back(name);
}

// Get the 1-dim domain name
vector<string> ConfigData::getDomainName(){
    return domainName;
}

// Get the 2-dim project name
vector<vector<string>> ConfigData::getProjectName(){
    return projectName;
}

// Print the static members
void ConfigData::printName(){
    for(unsigned i = 0; i < domainName.size(); i++){
        cout<<domainName[i]<<": ";
        for(unsigned j = 0; j < projectName[i].size(); j++){
            cout<<projectName[i][j];
            if(j != projectName[i].size()-1)
                cout<<", ";
            else
                cout<<"\n";
        }
    }
    return;
}

//===----------------------------------------------------------------------===//
//
//                     CallData Class
//
//===----------------------------------------------------------------------===//
// This class is designed to deal with the data of function and post-brance calls.
// All the data will be stored into database.
//===----------------------------------------------------------------------===//

ConfigData CallData::configData;
sqlite3* CallData::db;

// Open the SQLite database
void CallData::openDatabase(string file){
    if(file.empty()){
        fprintf(stderr, "The database file is empty!\n");
        exit(1);
    }
    
    int rc = sqlite3_open(file.c_str(), &db);
    if(rc){
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }
    
    sqlite3_busy_timeout(db, 10*1000); // wait for 10s when the database is locked
}

// Close the SQLite database
void CallData::closeDatabase(){
    sqlite3_close(db);
}

// Callback function to get the selected data in  SQLite
static int cb_get_info(void *data, int argc, char **argv, char **azColName){
    
    pair<int, vector<string>>* rowdata = (pair<int, vector<string>>*) data;
    
    // Multiple rows have the same function name, which should not happen.
    if(rowdata->first != 0){
        fprintf(stderr, "Multiple rows have the same function name\n");
        return SQLITE_ERROR;
    }
    
    // Print the row
    //int i;
    //printf("Selected: ");
    //for(i=0; i<argc; i++){
    //    printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    //}
    //printf("\n");
    
    // Get the values
    vector<string> values;
    for(int i = 0; i < argc; i++){
        string value = argv[i];
        values.push_back(value);
    }
    
    // Store the ID and values
    *rowdata = make_pair(atoi(argv[0]), values);
    return SQLITE_OK;
}

// Add a branch call
void CallData::addBranchCall(BranchInfo branchInfo){
    
    // Get the domain name and project name from given path
    pair<string, string> mDomProName = getDomainProjectName(branchInfo.callID);
    string domainName = mDomProName.first;
    string projectName = mDomProName.second;
    if(domainName.empty() || projectName.empty()){
        return;
    }
    
    // Prepare the sql stmt to create the table
    int rc;
    char *zErrMsg = 0;
    string stmt = "create table if not exists branch_call (ID integer primary key autoincrement, DomainName text, ProjectName text, CallName text, CallDefLoc text, CallID text, CallStr text, CallReturn text, CallArgVec text, CallArgNum integer, ExprNodeVec text, ExprNodeNum integer, ExprStr text, LogName text, LogDefLoc text, LogID text, LogStr text, LogArgVec text, LogArgNum integer)";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    // Prepare the sql stmt to insert new entry
    string callArgVecStr;
    for(unsigned i = 0; i < branchInfo.callArgVec.size(); i++){
        callArgVecStr += branchInfo.callArgVec[i];
        if(i != branchInfo.callArgVec.size() - 1)
            callArgVecStr += "#";
    }
    if(callArgVecStr == "")
        callArgVecStr = "-";
    string exprNodeVecStr;
    for(unsigned i = 0; i < branchInfo.exprNodeVec.size(); i++){
        exprNodeVecStr += branchInfo.exprNodeVec[i];
        if(i != branchInfo.exprNodeVec.size() - 1)
            exprNodeVecStr += "#";
    }
    if(exprNodeVecStr == "")
        exprNodeVecStr = "-";
    string logArgVecStr;
    for(unsigned i = 0; i < branchInfo.logArgVec.size(); i++){
        logArgVecStr += branchInfo.logArgVec[i];
        if(i != branchInfo.logArgVec.size() - 1)
            logArgVecStr += "#";
    }
    if(logArgVecStr == "")
        logArgVecStr = "-";
    
    char callArgNumStr[10];
    char exprNodeNumStr[10];
    char logArgNumStr[10];
    sprintf(callArgNumStr, "%lu", branchInfo.callArgVec.size());
    sprintf(exprNodeNumStr, "%lu", branchInfo.exprNodeVec.size());
    sprintf(logArgNumStr, "%lu", branchInfo.logArgVec.size());

    stmt = "insert into branch_call (DomainName, ProjectName, CallName, CallDefLoc, CallID, CallStr, CallReturn, CallArgVec, CallArgNum, ExprNodeVec, ExprNodeNum, ExprStr, LogName, LogDefLoc, LogID, LogStr, LogArgVec, LogArgNum) values ('" + domainName + "', '" + projectName + "', '" + branchInfo.callName + "', '" + branchInfo.callDefLoc + "', '" + branchInfo.callID + "', '" + branchInfo.callStr + "', '" + branchInfo.callReturn + "', '" + callArgVecStr + "', '" + callArgNumStr + "', '" + exprNodeVecStr + "', '" + exprNodeNumStr + "', '" + branchInfo.exprStr + "', '" + branchInfo.logName + "', '" + branchInfo.logDefLoc + "', '" + branchInfo.logID + "', '" + branchInfo.logStr + "', '" + logArgVecStr + "', '" + logArgNumStr +"')";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    return;
}

// Add a pre-branch call
void CallData::addPrebranchCall(string callName, string callLocFullPath, string callDefFullPath, string logName, string logDefFullPath){
    
    // Get the domain name and project name from given path
    pair<string, string> mDomProName = getDomainProjectName(callLocFullPath);
    string domainName = mDomProName.first;
    string projectName = mDomProName.second;
    if(domainName.empty() || projectName.empty()){
        return;
    }
    
    // Prepare the sql stmt to create the table
    int rc;
    char *zErrMsg = 0;
    string stmt = "create table if not exists prebranch_call (ID integer primary key autoincrement, CallName text, CallDefLoc text, DomainName text, ProjectName text, LogName text, LogDefLoc text, NumLogTime integer)";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    // Check whether the function is called the first time
    int ID = 0;
    vector<string> values;
    values.clear();
    // Store ID and values for the each row
    pair<int, vector<string>> rowdata = make_pair(ID, values);
    stmt="select * from prebranch_call where LogName = '" + logName + "' and LogDefLoc = '" + logDefFullPath + "' and CallName = '" + callName + "' and CallDefLoc = '" + callDefFullPath + "' and DomainName = '" + domainName + "' and ProjectName = '" + projectName + "'";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), cb_get_info, &rowdata, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    // The function is called the first time
    if(rowdata.first == 0){
        
        // Prepare the sql stmt to insert new entry
        stmt = "insert into prebranch_call (CallName, CallDefLoc, DomainName, ProjectName, LogName, LogDefLoc, NumLogTime) values ('" + callName + "', '" + callDefFullPath + "', '" + domainName + "', '" + projectName + "', '" + logName + "', '" + logDefFullPath + "', 1)";
        if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
        rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
        if(rc!=SQLITE_OK){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    else{
        // Prepare the sql stmt to update the entry
        //get old value
        int numLogTime =  atoi(rowdata.second[7].c_str());
        //get new value
        numLogTime++;
        //convert from int to char*
        char numLogTimeStr[10];
        sprintf(numLogTimeStr, "%d", numLogTime);
        string numLogTimeString = numLogTimeStr;
        //combine whole sql statement
        stmt = "update prebranch_call set NumLogTime = " + numLogTimeString + " where LogName = '" + logName + "' and LogDefLoc = '" + logDefFullPath + "' and CallName = '" + callName + "' and CallDefLoc = '" + callDefFullPath + "' and DomainName = '" + domainName + "' and ProjectName = '" + projectName + "'";
        if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
        //execute the update stmt
        rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
        if(rc!=SQLITE_OK){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    return;
}

// Add a post-branch call
void CallData::addPostbranchCall(string callName, string callLocFullPath, string callDefFullPath, string logName, string logDefFullPath){
    
    // Get the domain name and project name from given path
    pair<string, string> mDomProName = getDomainProjectName(callLocFullPath);
    string domainName = mDomProName.first;
    string projectName = mDomProName.second;
    if(domainName.empty() || projectName.empty()){
        return;
    }
    
    // Prepare the sql stmt to create the table
    int rc;
    char *zErrMsg = 0;
    string stmt = "create table if not exists postbranch_call (ID integer primary key autoincrement, LogName text, LogDefLoc text, DomainName text, ProjectName text, PrebranchCall text, NumPrebranchCall integer, NumPostbranchCall integer)";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    // Check whether the function is called the first time
    int ID = 0;
    vector<string> values;
    values.clear();
    // Store ID and values for the certain row
    pair<int, vector<string>> rowdata = make_pair(ID, values);
    stmt="select * from postbranch_call where LogName = '" + logName + "' and LogDefLoc = '" + logDefFullPath + "' and DomainName = '" + domainName + "' and ProjectName = '" + projectName + "'";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), cb_get_info, &rowdata, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    // The function is called the first time
    if(rowdata.first == 0){

        // Prepare the sql stmt to insert new entry
        stmt = "insert into postbranch_call (LogName, LogDefLoc, DomainName, ProjectName, PrebranchCall, NumPrebranchCall, NumPostbranchCall) values ('" + logName + "', '" + logDefFullPath + "', '" + domainName + "', '" + projectName + "', '#" + callName + "#', 1, 1)";
        if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
        rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
        if(rc!=SQLITE_OK){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    else{
        // Prepare the sql stmt to update the entry
        //get old value
        string prebranchCall = rowdata.second[5];
        int numPrebranchCall =  atoi(rowdata.second[6].c_str());
        int numPostbranchCall =  atoi(rowdata.second[7].c_str());
        //get new value
        if(prebranchCall.find("#" + callName + "#") == string::npos){
            prebranchCall += callName + "#";
            numPrebranchCall++;
        }
        numPostbranchCall++;
        //convert from int to char*
        char numPrebranchCallStr[10];
        char numPostbranchCallStr[10];
        sprintf(numPrebranchCallStr, "%d", numPrebranchCall);
        sprintf(numPostbranchCallStr, "%d", numPostbranchCall);
        //combine whole sql statement
        stmt = "update postbranch_call set PrebranchCall = '" + prebranchCall + "', NumPrebranchCall = " + numPrebranchCallStr + ", NumPostbranchCall = " + numPostbranchCallStr + " where LogName = '" + logName + "' and LogDefLoc = '" + logDefFullPath + "' and DomainName = '" + domainName + "' and ProjectName = '" + projectName + "'";
        if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
        //execute the update stmt
        rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
        if(rc!=SQLITE_OK){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    return;
}

// Add an edge of call graph
void CallData::addCallGraph(string funcName, string funcDefFullPath, string callName, string callDefFullPath, string callLocFullPath){
    
    // Get the domain name and project name from given path
    pair<string, string> mDomProName = getDomainProjectName(callLocFullPath);
    string domainName = mDomProName.first;
    string projectName = mDomProName.second;
    if(domainName.empty() || projectName.empty()){
        return;
    }
    
    // Prepare the sql stmt to create the table
    int rc;
    char *zErrMsg = 0;
    string stmt = "create table if not exists call_graph (ID integer primary key autoincrement, FuncName text, FuncDefLoc text, DomainName text, ProjectName text, CallName text, CallDefLoc integer)";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
        
    // Prepare the sql stmt to insert new entry
    stmt = "insert into call_graph (FuncName, FuncDefLoc, DomainName, ProjectName, CallName, CallDefLoc) values ('" + funcName + "', '" + funcDefFullPath + "', '" + domainName + "', '" + projectName + "', '" + callName + "', '" + callDefFullPath +"')";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return;
}

// Add a function call
void CallData::addFunctionCall(string callName, string callLocFullPath, string callDefFullPath){
    
    // Get the domain name and project name from given path
    pair<string, string> mDomProName = getDomainProjectName(callLocFullPath);
    string domainName = mDomProName.first;
    string projectName = mDomProName.second;
    if(domainName.empty() || projectName.empty()){
        return;
    }
    
    // Prepare the sql stmt to create the table
    int rc;
    char *zErrMsg = 0;
    string stmt = "create table if not exists call_statistic (ID integer primary key autoincrement, CallName text, CallDefLoc text, DomainName text, ProjectName text, CallNumber integer)";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    // Check whether the function is called the first time
    int ID = 0;
    vector<string> values;
    values.clear();
    // Store ID and values for the certain row
    pair<int, vector<string>> rowdata = make_pair(ID, values);
    stmt="select * from call_statistic where CallName = '" + callName + "' and CallDefLoc = '" + callDefFullPath + "' and DomainName = '" + domainName + "' and ProjectName = '" + projectName + "'";
    if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
    rc = sqlite3_exec(db, stmt.c_str(), cb_get_info, &rowdata, &zErrMsg);
    if(rc!=SQLITE_OK){
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    
    // The function is called the first time
    if(rowdata.first == 0){
        
        // Prepare the sql stmt to insert new entry
        stmt = "insert into call_statistic (CallName, CallDefLoc, DomainName, ProjectName, CallNumber) values ('" + callName + "', '" + callDefFullPath + "', '" + domainName + "', '" + projectName + "' , 1)";
        if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
        rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
        if(rc!=SQLITE_OK){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    else{
        // Prepare the sql stmt to update the entry
        //get old value
        int callnumber =  atoi(rowdata.second[5].c_str());
        //get new value
        callnumber++;
        //convert from int to char*
        char callnumberStr[10];
        sprintf(callnumberStr, "%d", callnumber);
        string callnumberString = callnumberStr;
        stmt = "update call_statistic set CallNumber = " + callnumberString + " where CallName = '" + callName + "' and CallDefLoc = '" + callDefFullPath + "' and DomainName = '" + domainName + "' and ProjectName = '" + projectName + "'";
        //execute the update stmt
        if(OUTPUT_SQL_STMT)cerr<<stmt<<endl;
        rc = sqlite3_exec(db, stmt.c_str(), 0, 0, &zErrMsg);
        if(rc!=SQLITE_OK){
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
    }
    return;
}

// Get the domain and project name from the full path of the file
pair<string, string> CallData::getDomainProjectName(string callLocation){
    
    // Get the domain and project name
    vector<string> mDomainName = configData.getDomainName();
    vector<vector<string>> mProjectName = configData.getProjectName();
    
    // For each domain
    for(unsigned i = 0; i < mDomainName.size(); i++){
        
        // The full path contains the domain name
        if(callLocation.find("/"+mDomainName[i]+"/") != string::npos){
            
            // For each project
            for(unsigned j = 0; j < mProjectName[i].size(); j++){
                
                // The full path contains the project name
                if(callLocation.find("/"+mProjectName[i][j]+"/") != string::npos){
                    
                    // Make the pair of doamin name and project name, and return
                    return make_pair(mDomainName[i], mProjectName[i][j]);
                }
            }
        }
    }
    
    // Return an empty pair
    return make_pair("", "");
}
