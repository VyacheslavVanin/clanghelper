#include "declprocessor.hpp"

#include <iostream>
#include <set>

#include "clanghelper/arghelper.hpp"
#include "clanghelper/stdhelper/containerhelper.hpp"
#include "clanghelper/vvvclanghelper.hpp"
#include <clang/Basic/Version.h>
#if CLANG_VERSION_MAJOR >= 6
#include <clang/Analysis/AnalysisDeclContext.h>
#else
#include <clang/Analysis/AnalysisContext.h>
#endif

using namespace vvv::helpers;

namespace {
using ParamList = std::vector<std::string>;

class MyASTConsumer : public clang::ASTConsumer {
public:
    explicit MyASTConsumer(clang::ASTContext* Context, const ParamList& params,
                           const decl_processor_t& processor)
        : params(params), processor(processor)
    {
    }

    ~MyASTConsumer() {}

    virtual bool HandleTopLevelDecl(clang::DeclGroupRef D) override
    {
        using namespace vvv::helpers;
        for (auto& decl : D)
            processor(decl);
        return true;
    }

    const ParamList& params;
    const decl_processor_t& processor;
};

class Tool : public clang::ASTFrontendAction {
public:
    Tool(const ParamList& params, const decl_processor_t& processor)
        : params(params), processor(processor)
    {
    }

    virtual std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(clang::CompilerInstance& Compiler, llvm::StringRef InFile)
    {
        return std::unique_ptr<clang::ASTConsumer>(
            new MyASTConsumer(&Compiler.getASTContext(), params, processor));
    }

    const ParamList& params;
    const decl_processor_t& processor;
};
} // namespace

void visit_decls(int argc, const char** argv, const decl_processor_t& f,
                 const std::vector<std::string>& my_params)
{
    if (argc < 2) {
        std::cerr << "fatal error: no input files" << std::endl;
    }

    const auto params = CxxToolArgs(argc, argv, my_params);

    for (const auto& name : params.getFilenames()) {
        using namespace clang::tooling;
        const auto code = getSourceFromFile(name.c_str());
        const auto& flags = params.getFlagsForSource(name);
        const auto& tool_flags = params.getCustomFlags();
        runToolOnCodeWithArgs(new Tool(tool_flags, f), code.c_str(), flags,
                              name.c_str());
    }
}
