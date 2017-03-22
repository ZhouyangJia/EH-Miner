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


CallInfo::CallInfo(){
    totalCallNumber = 0;
    callProject = 0;
    memset(callNumber, 0, MAX_PROJECT);
    
    defLocation = "";
    multiDef = false;
    
    outProjectDef = false;
}

// Method and member in CallData
// The memebers are static, and used to store all the call information
ConfigData CallData::configData;
int CallData::totalIndex;
map<string, int> CallData::call2index;
vector<CallInfo> CallData::callInfoVec;

void CallData::addCallExpression(string callName, string callLocation, string defLocation){
    
    string domainName = getDomainName(callLocation);
    string projectName = getProjectName(callLocation);
    int projectID = getProjectID(domainName, projectName);
    
    if(call2index[callName] == 0){
        totalIndex++;
        call2index[callName] = totalIndex;
        
        CallInfo callInfo;
        
        callInfo.totalCallNumber++;
        if(callInfo.callNumber[projectID] == 0)
            callInfo.callProject++;
        callInfo.callNumber[projectID]++;
        
        callInfo.defLocation = defLocation;
        callInfo.multiDef = false;
        
        callInfo.outProjectDef = isOutProjectDef(callLocation, projectName);
        
        callInfoVec.push_back(callInfo);
    }
    else{
        int curIndex = call2index[callName];
        
        callInfoVec[curIndex].totalCallNumber++;
        if(callInfoVec[curIndex].callNumber[projectID] == 0)
            callInfoVec[curIndex].callProject++;
        callInfoVec[curIndex].callNumber[projectID]++;
        
        if(callInfoVec[curIndex].defLocation != defLocation)
            callInfoVec[curIndex].multiDef = true;
    }
    
    return;
}

string CallData::getDomainName(string callLocation){
    
    return "";
}

string CallData::getProjectName(string callLocation){
    
    return "";
}

int CallData::getProjectID(string domainName, string projectName){
    
    return 0;
}

bool CallData::isOutProjectDef(string callLocation, string projectName){
    
    return false;
}
