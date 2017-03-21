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


int main(int argc, const char **argv) {
    
    CommonOptionsParser OptionsParser(argc, argv, ClangMytoolCategory);
    vector<string> source = OptionsParser.getSourcePathList();
    
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
