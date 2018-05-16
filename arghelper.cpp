#include "arghelper.hpp"
#include "stdhelper/containerhelper.hpp"
#include "vvvclanghelper.hpp"

using string_list = CxxToolArgs::string_list;
using namespace vvv::helpers;

std::vector<std::string> argstoarray(int argc, const char** argv)
{
    return std::vector<std::string>(&argv[1], &argv[argc]);
}

std::vector<std::string> filterParams(const std::vector<std::string>& in)
{
    auto beginWithMinus = [](const std::string& s) { return s[0] == '-'; };
    return filter(in, beginWithMinus);
}

std::vector<std::string> filterSourceFiles(const std::vector<std::string>& in,
                                           const std::vector<std::string>& exts)
{
    auto endWith = [&exts](const std::string& s) {
        return any_of(exts, [&s](const auto& ext) {
            return s.rfind(ext) != std::string::npos;
        });
    };
    return filter(in, endWith);
}

std::vector<std::string>
filterNotSourceFiles(const std::vector<std::string>& in,
                     const std::vector<std::string>& exts)
{
    auto not_source = [&exts](const std::string& s) {
        return std::all_of(exts.begin(), exts.end(), [&s](const auto& ext) {
            return s.rfind(ext) == std::string::npos;
        });
    };
    return filter(in, not_source);
}

namespace {
std::vector<std::string> autoDetectFlags(const std::string& filename)
{
    using clang::tooling::CompilationDatabase;

    char path[PATH_MAX];
    realpath(filename.data(), path);

    std::string error;
    const auto compile_db =
        CompilationDatabase::autoDetectFromSource(path, error);
    if (!compile_db) {
        std::cerr << error << "\n";
        return {};
    }

    const auto& ccs = compile_db->getCompileCommands(path);
    if (ccs.empty())
        return {};

    const auto& cc = ccs.front();
    const auto& cl = cc.CommandLine;
    if (cl.size() <= 5)
        return {};

    // remove command (/usr/bin/c++) and -c ..foo.cpp -o ..foo.o
    return std::vector<std::string>(cl.begin() + 1, cl.end() - 4);
}

std::string dump_process_cout(const std::string& command)
{
    std::string ret;
    FILE* fd = popen(command.data(), "r");
    if (!fd)
        return ret;

    const size_t buff_size = 256;
    char buf[buff_size];
    while (true) {
        const size_t bytes_read = fread(buf, 1, buff_size, fd);
        if (bytes_read < 1)
            break;

        ret.append(buf, bytes_read);

        if (bytes_read < buff_size)
            break;
    }
    fclose(fd);

    const auto i = ret.rfind("\n");
    if (i != std::string::npos)
        ret.resize(i);

    return ret;
}

void substitute(std::string& text, const std::string& search,
                const std::string& replace)
{
    auto i = text.find(search);
    if (i == std::string::npos)
        return;

    const auto size = search.size();
    text.replace(i, size, replace);
}

void addInclude(string_list& list, const std::string& path,
                const std::string& gcc_version)
{
    std::string p = path;
    substitute(p, "{}", gcc_version);
    list.push_back("-isystem");
    list.push_back(std::move(p));
}

/**
 * @brief Crutch to avoid such errors:<br>
 * <pre>/usr/include/stdio.h:33:10: fatal error: 'stddef.h' file not found</pre>
 */
string_list makeDefaultCompilerIncludes()
{
    static const auto& gcc_version = dump_process_cout("gcc -dumpversion");
    static const auto paths = {
        "/usr/include/c++/{}/",
        "/usr/include/c++/{}/x86_64-pc-linux-gnu",
        "/usr/lib/gcc/x86_64-pc-linux-gnu/{}/include",
        "/usr/lib/gcc/x86_64-pc-linux-gnu/{}/include-fixed",
        "/usr/include",
        "/usr/local/include",
    };

    string_list ret;
    for (const auto& p : paths)
        addInclude(ret, p, gcc_version);

    return ret;
}

} // namespace

CxxToolArgs::CxxToolArgs(int argc, const char** argv,
                         const string_list& tool_flags)
{
    static const auto default_includes = makeDefaultCompilerIncludes();

    const auto myParamFilter = [&tool_flags](const auto& p) {
        return contain(tool_flags, p);
    };
    const auto notMyParamsFilter = [&myParamFilter](const auto& p) {
        return !myParamFilter(p);
    };

    args = argstoarray(argc, argv);

    const auto allParams = filterNotSourceFiles(args);
    custom_params = filter(allParams, myParamFilter);
    compiler_params = filter(allParams, notMyParamsFilter) + default_includes;
    filenames = filterSourceFiles(args);
}

string_list
CxxToolArgs::getFlagsForSource(const std::string& source_file_name) const
{
    const auto& compile_commands = autoDetectFlags(source_file_name);
    return compile_commands.size() ? compile_commands + compiler_params
                                   : compiler_params;
}

const string_list& CxxToolArgs::getCustomFlags() const { return custom_params; }

const string_list& CxxToolArgs::getFilenames() const { return filenames; }

const string_list& CxxToolArgs::getCompilerParams() const
{
    return compiler_params;
}

const string_list& CxxToolArgs::getArgs() const { return args; }
