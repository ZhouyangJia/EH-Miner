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
#include <iostream>
#include <string.h>

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


class CallData{
    
};

#endif /* DataUtility_h */
