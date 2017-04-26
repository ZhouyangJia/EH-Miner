//===- DataUtility.h - A utility class used for handling function call -===//
//
//                     Cross Project Checking
//
// Author: Zhouyang Jia, PhD Candidate
// Affiliation: School of Computer Science, National University of Defense Technology
// Email: jiazhouyang@nudt.edu.cn
//
//===----------------------------------------------------------------------===//
//
// This file implements a utility class used for handling function call.
//
//===----------------------------------------------------------------------===//

#ifndef DataUtility_h
#define DataUtility_h

#include <vector>
#include <map>

#include <iostream>
#include <string.h>

#include <sqlite3.h>

#define MAX_PROJECT 100

#define OUTPUT_SQL_STMT 0

using namespace std;

//===----------------------------------------------------------------------===//
//
//                     ConfigData Class
//
//===----------------------------------------------------------------------===//
// This class stores the data got from config file, including the domain
// information and projects in each domain. We use two static members to
// store the information so that it can be used all around our project.
//===----------------------------------------------------------------------===//
class ConfigData{
public:
    // Add a domain name from config file
    void addDomainName(string name);
    
    // Add a project in domain numbered domainIndex
    void addProjectName(unsigned domainIndex, string name);
    
    // Get the 1-dim domain name
    vector<string> getDomainName();
    
    // Get the 2-dim project name
    vector<vector<string>> getProjectName();
    
    // Print the static members
    void printName();
    
private:
    // Store the domain information
    static vector<string> domainName;
    
    // Store the project information by a two-dimention vector
    static vector<vector<string>> projectName;
};

//===----------------------------------------------------------------------===//
//
//                     BranchInfo Struct
//
//===----------------------------------------------------------------------===//
// This class stores the data of information around a branch statement. When the
// branch is data dependent on a callexpr, we declare an instance to store its
// context, including callexpr, branch condition and the following function calls
//===----------------------------------------------------------------------===//
struct BranchInfo{
    
    string callName;
    string callDefLoc;
    string callID;
    string callStr;
    string callReturn;
    vector<string> callArgVec;
    
    vector<string> exprNodeVec; //reverse Polish notation
    string exprStr;
    
    string logName;
    string logDefLoc;
    string logID;
    string logStr;
    vector<string> logArgVec;
    
};

//===----------------------------------------------------------------------===//
//
//                     CallData Class
//
//===----------------------------------------------------------------------===//
// This class is designed to deal with the data of function and post-brance calls.
// All the data will be stored into database.
//===----------------------------------------------------------------------===//

class CallData{
public:
    // Add a function call
    void addFunctionCall(string callName, string callLocFullPath, string callDefFullPath);
    
    // Add an edge of call graph
    void addCallGraph(string funcName, string funcDefFullPath, string callName, string callDefFullPath, string callLocFullPath);
    
    // Add a post-branch call
    void addPostbranchCall(string callName, string callLocFullPath, string callDefFullPath, string logName, string logDefFullPath);
    
    // Add a pre-branch call
    void addPrebranchCall(string callName, string callLocFullPath, string callDefFullPath, string logName, string logDefFullPath);
    
    // Add a branch call
    void addBranchCall(BranchInfo branchInfo);
    
    // Open the SQLite database
    void openDatabase(string databasefile);
    
    // Close the SQLite database
    void closeDatabase();

private:
    // Get the domain and project name from the full path of the file
    pair<string, string> getDomainProjectName(string callLocation);
    
    // The name of libconfig config file
    static ConfigData configData;
    
    // The SQLite database
    static sqlite3 *db;
};

#endif /* DataUtility_h */
