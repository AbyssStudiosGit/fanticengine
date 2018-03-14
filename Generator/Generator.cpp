//
// Copyright (c) 2008-2018 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <fstream>
#include <thread>
#include <cppast/libclang_parser.hpp>
#include <CLI11/CLI11.hpp>
#include <Urho3D/Urho3DAll.h>
#include "Pass/BuildMetaAST.h"
#include "Pass/UnknownTypesPass.h"
#include "Pass/CSharp/Urho3DTypeMaps.h"
#include "Pass/CSharp/MoveGlobalsPass.h"
#include "Pass/CSharp/ConvertToPropertiesPass.h"
#include "Pass/CSharp/ImplementInterfacesPass.h"
#include "Pass/CSharp/Urho3DCustomPass.h"
#include "Pass/CSharp/GenerateClassWrappers.h"
#include "Pass/CSharp/GenerateCApiPass.h"
#include "Pass/CSharp/GeneratePInvokePass.h"
#include "Pass/CSharp/GenerateCSApiPass.h"

namespace Urho3D
{

GeneratorContext* generator = nullptr;

}

int main(int argc, char* argv[])
{
    String rulesFile;
    String sourceDir;
    String outputDirCpp;
    String outputDirCs;
    std::vector<std::string> includes;
    std::vector<std::string> defines;
    std::vector<std::string> options;
    SharedPtr<Context> context;

    CLI::App app{"CSharp bindings generator"};

    app.add_option("-I", includes, "Target include paths.");
    app.add_option("-D", defines, "Target preprocessor definitions.");
    app.add_option("-O", options, "Target compiler options.");
    app.add_option("--out-cpp", outputDirCpp, "Output directory for generated C++ code.");
    app.add_option("--out-cs", outputDirCs, "Output directory for generated C# code.");

    app.add_option("rules", rulesFile, "Path to rules xml file")->check(CLI::ExistingFile);
    app.add_option("source", sourceDir, "Path to source directory")->check(CLI::ExistingDirectory);

    std::vector<std::string> cmdLines;
    if (argc == 2)
    {
        std::ifstream infile(argv[1]);
        std::string line;
        while (std::getline(infile, line))
        {
            if (!line.empty())
                cmdLines.push_back(line);
        }

        char** newArgv = new char*[cmdLines.size()];
        auto newArgvStart = newArgv;
        newArgv[0] = argv[0];
        newArgv++;
        for (const auto& ln : cmdLines)
        {
            *newArgv = (char*)ln.c_str();
            newArgv++;
        }
        argv = newArgvStart;
        argc = cmdLines.size() + 1;
    }

    CLI11_PARSE(app, argc, argv);

    sourceDir = AddTrailingSlash(sourceDir);
    outputDirCpp = AddTrailingSlash(outputDirCpp);
    outputDirCs = AddTrailingSlash(outputDirCs);

    context = new Context();
    context->RegisterSubsystem(new FileSystem(context));
    context->RegisterSubsystem(new Log(context));
    context->RegisterSubsystem(new WorkQueue(context));
    context->GetLog()->SetLevel(LOG_DEBUG);
    context->GetWorkQueue()->CreateThreads(std::thread::hardware_concurrency());

    context->GetFileSystem()->CreateDirsRecursive(outputDirCpp);
    context->GetFileSystem()->CreateDirsRecursive(outputDirCs);

    // Generate bindings
    generator = new GeneratorContext(context);
    context->RegisterSubsystem(generator);

    generator->LoadCompileConfig(includes, defines, options);
#if _WIN32
    generator->config_.set_flags(cppast::cpp_standard::cpp_11, {
        cppast::compile_flag::ms_compatibility | cppast::compile_flag::ms_extensions
    });
#else
    generator->config_.set_flags(cppast::cpp_standard::cpp_11, {cppast::compile_flag::gnu_extensions});
#endif

    generator->LoadRules(rulesFile);
    generator->ParseFiles(sourceDir);

    generator->AddCppPass<BuildMetaAST>();
    generator->AddApiPass<Urho3DTypeMaps>();
    generator->AddApiPass<UnknownTypesPass>();
    generator->AddApiPass<GenerateClassWrappers>();
    generator->AddApiPass<Urho3DCustomPass>();
    generator->AddApiPass<MoveGlobalsPass>();
    generator->AddApiPass<ConvertToPropertiesPass>();
    generator->AddApiPass<ImplementInterfacesPass>();
    generator->AddApiPass<GenerateCApiPass>();
    generator->AddApiPass<GeneratePInvokePass>();
    generator->AddApiPass<GenerateCSApiPass>();

    generator->Generate(outputDirCpp, outputDirCs);
}
