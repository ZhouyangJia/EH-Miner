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
//                     CallInfo Struct
//
//===----------------------------------------------------------------------===//
// In this struct, we store all the needed information for all the functions
// called in all projects. Each instance handles one function, including the
// function name, call times, def and use location, etc.
//===----------------------------------------------------------------------===//

// Init for CallInfo, each instance of CallInfor stores infomation for one call
// The meaning of each member can be found in DataUtility.h
CallInfo::CallInfo(){
    callName = "";
    totalCallNumber = 0;
    callProject = 0;
    memset(callNumber, 0, MAX_PROJECT*sizeof(int));
    defLocation = "";
    multiDef = false;
    outProjectDef = false;
}

// Print the call infomation
void CallInfo::print(){
    // callName, callProject, totalCallNumber, outProjectDef, multiDef, callNumber, defLocation
    cout<<callName<<", "<<callProject<<", "<<totalCallNumber<<", "<<outProjectDef<<", "<<multiDef<<", ";
    for(int i = 0; i < MAX_PROJECT; i++){
        cout<<callNumber[i]<<", ";
    }
    cout<<defLocation<<endl;
    return;
}

//===----------------------------------------------------------------------===//
//
//                     CallData Class
//
//===----------------------------------------------------------------------===//
// This class is designed to store the call infomation of all functions. Since
// each FroentAction instance may have fucntion calls to store, all the members
// are static so that they will be shared by all FroentAction instances.
//===----------------------------------------------------------------------===//

// The memebers are static, and used to store all the call information
// The meaning of each member can be found in DataUtility.h
ConfigData CallData::configData;
int CallData::totalIndex;
map<string, int> CallData::call2index;
vector<CallInfo> CallData::callInfoVec;

// Add a call expression
void CallData::addCallExpression(string callName, string callLocation, string defLocation){
    
    // Get the domain name and project name of given path
    pair<string, string> mDomProName = getDomainProjectName(callLocation);
    string domainName = mDomProName.first;
    string projectName = mDomProName.second;
    if(domainName.empty() || projectName.empty()){
        cerr<<"Cannot find domain or project name for "<<callName<<":\n";
        cerr<<"\tcall@"<<callLocation<<"&"<<"def@"<<defLocation<<".\n";
        return;
    }
    
    // Get the project ID of given path
    int projectID = getProjectID(domainName, projectName);
    if(projectID == -1){
        cerr<<"Cannot find the ID of the given domain and project name: "<<domainName<<" and "<<projectName<<"!\n";
        return;
    }
    if(projectID >= MAX_PROJECT){
        cerr<<"Exceed the project limitation!\n";
        return;
    }
    
    // The function is called the first time
    if(call2index[callName] == 0){
        
        // Assign the index
        totalIndex++;
        call2index[callName] = totalIndex;
        
        // Declare the CallInfo instance, and init
        CallInfo callInfo;
        
        callInfo.callName = callName;
        callInfo.totalCallNumber++;
        if(callInfo.callNumber[projectID] == 0)
            callInfo.callProject++;
        callInfo.callNumber[projectID]++;
        callInfo.defLocation = defLocation;
        callInfo.multiDef = false;
        callInfo.outProjectDef = isOutProjectDef(defLocation, domainName, projectName);
        
        // Push the instance to callInfoVec
        callInfoVec.push_back(callInfo);
    }
    // The function has been called
    else{
        // Get the index (call2index is starting from 1)
        int curIndex = call2index[callName] - 1;
        
        // Update the infomation
        callInfoVec[curIndex].totalCallNumber++;
        if(callInfoVec[curIndex].callNumber[projectID] == 0)
            callInfoVec[curIndex].callProject++;
        callInfoVec[curIndex].callNumber[projectID]++;
        if(callInfoVec[curIndex].defLocation != defLocation)
            callInfoVec[curIndex].multiDef = true;
    }
    
    // test
    //cout<<callName<<"#"<<call2index[callName]<<"@"<<domainName<<":"<<projectName<<"#"<<projectID<<"\n";
    return;
}

// Print all the call infomation
void CallData::print(){
    
    for(int i = 1; i <= totalIndex; i++){
        cout<<i<<", ";
        callInfoVec[i-1].print();
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
        if(callLocation.find("/"+mDomainName[i]) != string::npos){
            
            // For each project
            for(unsigned j = 0; j < mProjectName[i].size(); j++){
                
                // The full path contains the project name
                if(callLocation.find("/"+mProjectName[i][j]) != string::npos){
                    
                    // Make the pair of doamin name and project name, and return
                    return make_pair(mDomainName[i], mProjectName[i][j]);
                }
            }
        }
    }
    
    // Return an empty pair
    return make_pair("", "");
}

// Get the project ID
int CallData::getProjectID(string domainName, string projectName){
    
    int ID = 0;
    
    // Get the domain and project name
    vector<string> mDomainName = configData.getDomainName();
    vector<vector<string>> mProjectName = configData.getProjectName();
    
    // For each domain
    for(unsigned i = 0; i < mDomainName.size(); i++){
        
        // The domain is equal to the parameter domainName
        if(domainName.find(mDomainName[i]) != string::npos){
            
            // For each project
            for(unsigned j = 0; j < mProjectName[i].size(); j++){
                
                // The domain is equal to the parameter projectName
                if(projectName.find(mProjectName[i][j]) != string::npos){
                    
                    // Add the index of project in current domain
                    ID += j;
                    return ID;
                }
            }
        }
        else{
            // Add the number of projects in this domain
            ID += mProjectName[i].size();
        }
    }
    
    // Fail to find the corresponding domain and project, which is not supposed to happen.
    return -1;
}

// Is the defination locates out of the project, aka, is used the third part library
bool CallData::isOutProjectDef(string defLocation, string domainName, string projectName){
    
    if(defLocation.find("/"+domainName) != string::npos && defLocation.find("/"+projectName) != string::npos){
        return false;
    }
    return true;
}
