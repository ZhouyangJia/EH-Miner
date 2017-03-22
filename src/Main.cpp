//===- Main.cpp - driver of the cross project checking tool-===//
//
//                     Cross Project Checking
//
// Author: Zhouyang Jia, PhD Candidate
// Affiliation: School of Computer Science, National University of Defense Technology
// Email: jiazhouyang@nudt.edu.cn
//
//===----------------------------------------------------------------------===//
//
// This file implements the entrance of our cross project checking tool.
//
//===----------------------------------------------------------------------===//


#include "llvm/Support/raw_ostream.h"
#include "llvm/Option/OptTable.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Driver/Options.h"
#include "DummyAction.h"
#include "libconfig.h"

#define MAX_DOMAIN 100
#define MAX_PROJECT_PER_DOMAIN 100
#define MAX_NAME_LENGTH 100
#define DEFAULT_CONFIG_FILE "/Users/zhouyangjia/llvm-4.0.0.src/tools/clang/tools/clang-dummy/etc/test.conf"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;
using namespace std;


static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp(
                              "\tFor example, to run clang-dummy on all files in a subtree of the\n"
                              "\tsource tree, use:\n"
                              "\n"
                              "\t  find path/in/subtree -name '*.cpp'|xargs clang-dummy\n"
                              "\n"
                              "\tor using a specific build path:\n"
                              "\n"
                              "\t  find path/in/subtree -name '*.cpp'|xargs clang-dummy -p build/path\n"
                              "\n"
                              "\tNote, that path/in/subtree and current directory should follow the\n"
                              "\trules described above.\n"
                              "\n"
                              );

static cl::OptionCategory ClangMytoolCategory("clang-dummy options");

static std::unique_ptr<opt::OptTable> Options(createDriverOptTable());

static cl::opt<bool> FindFunctionCall("find-function-call",
                                         cl::desc("Find function calls."),
                                         cl::cat(ClangMytoolCategory));

static cl::opt<string> ConfigFile("config-file",
                                      cl::desc("Specify config file, the default one is /Users/zhouyangjia/llvm-4.0.0.src/tools/clang/tools/clang-dummy/etc/test.conf."),
                                      cl::cat(ClangMytoolCategory));


int initConfig(string config_file){
    
    config_t conf;
    config_init(&conf);
    
    int rc;
    rc = config_read_file(&conf, config_file.c_str());
    if(rc == CONFIG_FALSE){
        if(conf.error_type == CONFIG_ERR_FILE_IO)
            errs()<<"Fail to read the config file: "<<config_file<<".\n";
        if(conf.error_type == CONFIG_ERR_PARSE)
            errs()<<"Fail to parse the config file: "<<conf.error_text<<" in "<<conf.error_file<<":"<<conf.error_line<<".\n";
        return EXIT_FAILURE;
    }
    
    config_setting_t* domains_setting;
    domains_setting = config_lookup(&conf, "domains");
    if(domains_setting == NULL){
        errs()<<"Fail to find domains setting!\n";
        return EXIT_FAILURE;
    }
    
    unsigned domains_length;
    domains_length = config_setting_length(domains_setting);
    if(domains_length == 0){
        errs()<<"There is no domain to be analyzed!\n";
        return EXIT_FAILURE;
    }
    if(domains_length > MAX_DOMAIN){
        errs()<<"Too many domains, only the first "<<MAX_DOMAIN<<" domains will be analyzed!\n";
        domains_length = MAX_DOMAIN;
    }
   
    // for each domain
    for(unsigned i = 0; i < domains_length; i++){
        char domains_index[MAX_NAME_LENGTH * 2];
        sprintf(domains_index, "domains.[%d]", i);
        
        const char* domain_name;
        rc = config_lookup_string(&conf, domains_index, &domain_name);
        if(rc == CONFIG_FALSE){
            errs()<<"Fail to read domain name!\n";
            return EXIT_FAILURE;
        }
        if(strlen(domain_name) > MAX_NAME_LENGTH){
            errs()<<"The domain name is too long!\n";
            return EXIT_FAILURE;
        }
        
        config_setting_t* projects_setting;
        projects_setting = config_lookup(&conf, domain_name);
        if(projects_setting == NULL){
            continue;
        }
        
        unsigned projects_length;
        projects_length = config_setting_length(projects_setting);
        if(projects_length == 0){
            continue;
        }
        
        // for each project in domain.[i]
        for(unsigned j = 0; j < projects_length; j++){
            char projects_index[MAX_NAME_LENGTH * 2];
            sprintf(projects_index, "%s.[%d]", domain_name, j);
            
            const char* project_name;
            rc = config_lookup_string(&conf, projects_index, &project_name);
            if(rc == CONFIG_FALSE){
                errs()<<"Fail to read project name!\n";
                return EXIT_FAILURE;
            }
            if(strlen(project_name) > MAX_NAME_LENGTH){
                errs()<<"The project name is too long!\n";
                return EXIT_FAILURE;
            }
            
            outs()<<project_name<<"\n";
            //TODO
        }
    }
    
    config_destroy(&conf);
    return EXIT_SUCCESS;
}


int main(int argc, const char **argv){


    CommonOptionsParser OptionsParser(argc, argv, ClangMytoolCategory);
    vector<string> source = OptionsParser.getSourcePathList();

    // Set default config path
    if(ConfigFile.empty())
        ConfigFile = DEFAULT_CONFIG_FILE;
    
    // Init and parse config file
    int rc;
    rc = initConfig(ConfigFile);
    if(rc != EXIT_SUCCESS){
        exit(1);
    }
    
    // Start analyzing
    std::unique_ptr<FrontendActionFactory> FrontendFactory;
    if(FindFunctionCall){
        FrontendFactory = newFrontendActionFactory<DummyAction>();
        for(unsigned i = 0; i < source.size(); i++){
            vector<string> mysource;
            mysource.push_back(source[i]);
            llvm::errs()<<"["<<i+1<<"/"<<source.size()<<"]"<<" Now analyze the file: "<<mysource[0]<<"\n";
            ClangTool Tool(OptionsParser.getCompilations(), mysource);
            Tool.run(FrontendFactory.get());
        }
    }
    
    return 0;
}
