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
//                     CallData Class
//
//===----------------------------------------------------------------------===//
// This class is designed to deal with the data of function and post-brance calls.
// All the data will be stored into database.
//===----------------------------------------------------------------------===//

class CallData{
public:
    // Add a function call
    void addFunctionCall(string callName, string callLocation, string defLocation);
    
    // Add a post-branch call
    void addPostbranchCall(string callName, string callLocation, string defLocation, string logName);
    
    // Print all function call infomation
    void printFunctionCall();
    
    // Print all post-branch call infomation
    void printPostbranchCall();
    
    // Set the name of SQLite database
    void setDatabase(string name);

private:
    // Get the domain and project name from the full path of the file
    pair<string, string> getDomainProjectName(string callLocation);
    
    // Get the domain ID and project ID
    pair<int, int> getDomainProjectID(string domainName, string projectName);
    
    // Is the definition locates out of the project, aka, is used the third part library
    bool isOutProjectDef(string defLocation, string domainName, string projectName);
    
    // The name of libconfig config file
    static ConfigData configData;
    
    // The name of SQLite database file
    static string databaseName;
};

#endif /* DataUtility_h */
