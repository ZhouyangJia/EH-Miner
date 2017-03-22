//===- DataUtility.cpp - A utility class used for handling function call -===//
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


#include "DataUtility.h"


// Method and member in ConfigData
// The memebers are static, and used to store the information of config file
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
