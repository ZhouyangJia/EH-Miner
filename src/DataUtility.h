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

#define MAX_PROJECT 20

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
//                     CallInfo Struct
//
//===----------------------------------------------------------------------===//
// In this struct, we store all the needed information for all the functions
// called in all projects. Each instance handles one function, including the
// function name, call times, def and use location, etc.
//===----------------------------------------------------------------------===//
struct CallInfo{
    
    // Init the members for each instance
    CallInfo();
    
    // Print the call infomation
    void print();
    
    // Function name
    string callName;
    
    // Total called number
    int totalCallNumber;
    
    // Number of projects, which calls the function
    int callProject;
    
    // Called number, for each project
    int callNumber[MAX_PROJECT];
    
    // Defination location of the function
    string defLocation;
    
    // Is defined in multiple locations, supposed to be false
    bool multiDef;
    
    // Is the defination locates out of the project, aka, is used the third part library
    bool outProjectDef;
};


//===----------------------------------------------------------------------===//
//
//                     CallData Class
//
//===----------------------------------------------------------------------===//
// This class is designed to store the call infomation of all functions. Since
// each FroentAction instance may have fucntion calls to store, all the members
// are static so that they will be shared by all FroentAction instances.
//===----------------------------------------------------------------------===//

class CallData{
public:
    // Add a call expression
    void addCallExpression(string callName, string callLocation, string defLocation);
    
    // Print all the call infomation
    void print();

private:
    // Get the domain and project name from the full path of the file
    pair<string, string> getDomainProjectName(string callLocation);
    
    // Get the project ID
    int getProjectID(string domainName, string projectName);
    
    // Is the defination locates out of the project, aka, is used the third part library
    bool isOutProjectDef(string defLocation, string domainName, string projectName);
    
    // Config data with domain and project names
    static ConfigData configData;
    
    // Mapping the function name and an integer index
    static map<string, int> call2index;
    
    // For every new function, this variable will increase by 1, and assigned to the mapping.
    static int totalIndex;
    
    // Store all function info, indexed by the integer mapping of function name
    static vector<CallInfo> callInfoVec;
};

#endif /* DataUtility_h */
