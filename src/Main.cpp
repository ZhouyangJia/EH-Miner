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

#include "FindFunctionCall.h"
#include "FindPostbranchCall.h"
#include "DataUtility.h"

#include "libconfig.h"

#include <vector>
#include <cstdio>
#include <ctime>
#include <cstdlib>

#define MAX_DOMAIN 100
#define MAX_PROJECT_PER_DOMAIN 100
#define MAX_NAME_LENGTH 100
#define DEFAULT_CONFIG_FILE "/Users/zhouyangjia/llvm-4.0.0.src/tools/clang/tools/clang-dummy/etc/test.conf"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;
using namespace std;

// Print help messages
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
                              "-find-function-call\n"
                              "\tUsing this option, our tool will perfrom the find function call\n"
                              "\taction. At least one action should be performed.\n"
                              "\n"
                              "-find-postbranch-call\n"
                              "\tUsing this option, our tool will perfrom the find postbranch call\n"
                              "\taction. At least one action should be performed.\n"
                              "\n"
                              "-config-file <config-file> specify the config file containing domains and projects.\n"
                              "\tConfig the domains, and projects for each domain we want to analyze. \n"
                              "\tThe default file is in path/to/clang/tools/clang-dummy/etc/test.conf.\n"
                              "\n"
                              "-source-file <source-file> specify the file containing paths of all source files.\n"
                              "\tIf there are thousands of files to analyze, xargs will split the files,\n"
                              "\tand invoke clang-dummy sevral times to avoid the command line becoming\n"
                              "\ttoo long, so the data in memory will be lost. To avoid this, use:\n"
                              "\n"
                              "\t  find path/in/subtree -name '*.cpp' > all_files.in\n"
                              "\t  clang-dummy -p build/path -source-file=all_files.in empty.c\n"
                              "\n"
                              "\tIf there are multiple compile_commands.json files to be merged, use:\n"
                              "\n"
                              "\t  find . -name \"compile_commands.json\" | xargs jq 'flatten' -s > ./compile_commands.json\n"
                              "\n"
                              "\tIf there are multiple suffix files to be analyzed, use:\n"
                              "\n"
                              "\t  find . -name \"*.c\" -o -name \"*.cc\" -o -name \"*.cpp\"\n"
                              "\n"
                              "-database-file <database-file> specify the database (absolute path).\n"
                              "\tIf we do want to invoke the tool several times, then we need to store\n"
                              "\tthe data to database instead of memory. If the database file doesn't \n"
                              "\texist, the tool will create one. To use this feature, please complie \n"
                              "\tour tool with option -DSQLITE=ON, and use:\n"
                              "\n"
                              "\t  find path/in/subtree -name '*.cpp'| xargs clang-dummy -p build/path -database-file=/absolute/path/to/database.db\n"
                              "\n"
                              "\tor use: (there are too many source files)\n"
                              "\n"
                              "\t  find path/in/subtree -name '*.cpp' > all_files.in\n"
                              "\t  clang-dummy -p build/path -database-file=/absolute/path/to/database.db -source-file=all_files.in empty.c\n"
                              "\n"
                              "\tor use: (anslyze subdirection one by one)\n"
                              "\n"
                              "\t  find path/in/subtree/subdir1 -name '*.cpp'| xargs clang-dummy -p build/path -database-file=/absolute/path/to/database.db\n"
                              "\t  find path/in/subtree/subdir2 -name '*.cpp'| xargs clang-dummy -p build/path -database-file=/absolute/path/to/database.db\n"
                              "\n"
                              );

// Deal with command line options
static cl::OptionCategory ClangMytoolCategory("clang-dummy options");

static std::unique_ptr<opt::OptTable> Options(createDriverOptTable());

static cl::opt<bool> FindFunctionCall("find-function-call",
                                         cl::desc("Find function calls."),
                                      cl::cat(ClangMytoolCategory));

static cl::opt<bool> FindPostbranchCall("find-postbranch-call",
                                      cl::desc("Find post-branch calls."),
                                      cl::cat(ClangMytoolCategory));

static cl::opt<string> ConfigFile("config-file",
                                      cl::desc("Specify config file."),
                                      cl::cat(ClangMytoolCategory));

static cl::opt<string> DatabaseFile("database-file",
                                  cl::desc("Specify database file (absolute path)."),
                                  cl::cat(ClangMytoolCategory));


static cl::opt<string> SourceFile("source-file",
                                    cl::desc("Specify source file (default is path/to/clang/tools/clang-dummy/etc/test.conf."),
                                    cl::cat(ClangMytoolCategory));

// Read and parse config file, and then store the domain and project information to ConfigData class
int initConfig(string config_file){
    
    // Used to store the information of config file
    ConfigData configData;
    
    // Declare the libconfig object and init
    config_t conf;
    config_init(&conf);
    
    // Read config file, and handle I/O error and Parse error
    int rc;
    rc = config_read_file(&conf, config_file.c_str());
    if(rc == CONFIG_FALSE){
        if(conf.error_type == CONFIG_ERR_FILE_IO)
            errs()<<"Fail to read the config file: "<<config_file<<".\n";
        if(conf.error_type == CONFIG_ERR_PARSE)
            errs()<<"Fail to parse the config file: "<<conf.error_text<<" in "<<conf.error_file<<":"<<conf.error_line<<".\n";
        return EXIT_FAILURE;
    }
    
    // Fine "domains" setting which specify the domains to be analyzed
    config_setting_t* domains_setting;
    domains_setting = config_lookup(&conf, "domains");
    if(domains_setting == NULL){
        errs()<<"Fail to find domains setting!\n";
        return EXIT_FAILURE;
    }
    
    // Count the number of domains, and limit the max number to 100
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
    
    // For each domain
    for(unsigned i = 0; i < domains_length; i++){
        
        // Get the "path" to make libconfig happy
        char domains_index[MAX_NAME_LENGTH * 2];
        sprintf(domains_index, "domains.[%d]", i);
        
        // Read domain name through "path", and limit the its lenght
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
        
        // Store domain name to ConfigData class
        configData.addDomainName(domain_name);
        
        // Get the corresponding projects setting for each domain through domain name
        config_setting_t* projects_setting;
        projects_setting = config_lookup(&conf, domain_name);
        if(projects_setting == NULL){
            continue;
        }
        
        // Get the number of projects for the given domain
        unsigned projects_length;
        projects_length = config_setting_length(projects_setting);
        if(projects_length == 0){
            continue;
        }
        
        // For each project in domain.[i]
        for(unsigned j = 0; j < projects_length; j++){
            
            // Get the "path" to make libconfig happy
            char projects_index[MAX_NAME_LENGTH * 2];
            sprintf(projects_index, "%s.[%d]", domain_name, j);
            
            // Read project name through "path", and limit the its lenght
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
            
            // Store project name to ConfigData class
            configData.addProjectName(i, project_name);
        }
    }
    
    // Free libconfig object and exit success
    config_destroy(&conf);
    return EXIT_SUCCESS;
}

// Please read from the program entrance :)
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
    
    // If we specify the file with all source file names, we will analyze those files
    // instead of files in command line.
    if(!SourceFile.empty()){
        if(freopen(SourceFile.c_str(), "r", stdin) == NULL)
            fprintf(stderr,"error redirecting stdin\n");
        source.clear();
        char filename[200];
        while(scanf("%s\n",filename)!=EOF){
            source.push_back(filename);
        }
    }
    
    if(!DatabaseFile.empty()){
        CallData callData;
        callData.setDatabase(DatabaseFile);
    }
    
    if(!FindFunctionCall && !FindPostbranchCall){
        errs()<<"Please specify the action to do (e.g., -find-function-call, or -find-postbranch-call)!\n";
        exit(1);
    }
    
    // Start analyzing
    std::unique_ptr<FrontendActionFactory> FrontendFactory;
    
    if(FindFunctionCall){
        FrontendFactory = newFrontendActionFactory<FindFunctionCallAction>();
        
        // We analyze the source files one by one, since something weird happens when analyzing at once.
        // More details see http://lists.llvm.org/pipermail/cfe-dev/2015-April/042654.html
        for(unsigned i = 0; i < source.size(); i++){
            vector<string> mysource;
            mysource.push_back(source[i]);
            time_t now_time = time(NULL);
            struct tm* current_time = localtime(&now_time);
            llvm::errs()<<current_time->tm_hour<<":"<<current_time->tm_min<<":"<<current_time->tm_sec<<" ";
            llvm::errs()<<"["<<i+1<<"/"<<source.size()<<"]"<<" Find function call in: "<<mysource[0]<<"\n";
            ClangTool Tool(OptionsParser.getCompilations(), mysource);
            Tool.setDiagnosticConsumer(new IgnoringDiagConsumer());
            Tool.run(FrontendFactory.get());
        }
    }
    
    if(FindPostbranchCall){
        FrontendFactory = newFrontendActionFactory<FindPostbranchCallAction>();
        
        for(unsigned i = 0; i < source.size(); i++){
            vector<string> mysource;
            mysource.push_back(source[i]);
            time_t now_time = time(NULL);
            struct tm* current_time = localtime(&now_time);
            llvm::errs()<<current_time->tm_hour<<":"<<current_time->tm_min<<":"<<current_time->tm_sec<<" ";
            llvm::errs()<<"["<<i+1<<"/"<<source.size()<<"]"<<" Find Post-branch call in "<<mysource[0]<<"\n";
            ClangTool Tool(OptionsParser.getCompilations(), mysource);
            Tool.setDiagnosticConsumer(new IgnoringDiagConsumer());
            Tool.run(FrontendFactory.get());
        }
    }
    
    // Print the result only when not using database
    if(FindFunctionCall && DatabaseFile.empty()){
        CallData callData;
        callData.printFunctionCall();
    }
    
    if(FindPostbranchCall && DatabaseFile.empty()){
        CallData callData;
        callData.printPostbranchCall();
    }
    return 0;
}
