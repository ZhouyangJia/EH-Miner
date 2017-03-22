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

#define MAX_PROJECT 100

using namespace std;

// This class stores the data got from config file, including the domain
// information and projects in each domain. We use two static members to
// store the information so that it can be used all around our project.
class ConfigData{
public:
    // Add a domain name from config file
    void addDomainName(string name);
    
    // Add a project in domain numbered domainIndex
    void addProjectName(unsigned domainIndex, string name);
    
    // Print the static members
    void printName();
    
private:
    // Store the domain information
    static vector<string> domainName;
    
    // Store the project information by a two-dimention vector
    static vector<vector<string>> projectName;
};

struct CallInfo{
    
    CallInfo();
    
    int totalCallNumber;
    int callProject;
    int callNumber[MAX_PROJECT];
    
    string defLocation;
    bool multiDef;
    
    bool outProjectDef;
};

class CallData{
public:
    // Add a call expression
    void addCallExpression(string callName, string callLocation, string defLocation);

private:
    string getDomainName(string callLocation);
    
    string getProjectName(string callLocation);
    
    int getProjectID(string domainName, string projectName);
    
    bool isOutProjectDef(string callLocation, string projectName);
    
    static ConfigData configData;
    static map<string, int> call2index;
    static int totalIndex;
    static vector<CallInfo> callInfoVec;
};

#endif /* DataUtility_h */
