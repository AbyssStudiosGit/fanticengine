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

#pragma once


#include <regex>
#include <Urho3D/Container/Str.h>
#include <cppast/cpp_entity.hpp>
#include <Urho3D/Resource/JSONValue.h>
#include <cppast/cpp_type.hpp>
#include <cppast/cpp_function.hpp>
#include <cppast/cpp_member_function.hpp>


namespace Urho3D
{

/// Convert a wildcard string to regular expression. "*" matches anything except /, "**" matches everything including /.
std::regex WildcardToRegex(const String& wildcard);
/// Returns entity name including names of it's parent scopes (separated by ::). Also includes function signature.
std::string GetUniqueName(const cppast::cpp_entity& e);
/// Returns entity name including names of it's parent scopes (separated by ::).
std::string GetScopeName(const cppast::cpp_entity& e);
/// Returns entity name including names of it's parent scopes (separated by ::).
std::string GetUniqueName(const cppast::cpp_entity* e);
/// Returns entity name including names of it's parents (separated by ::).
std::string GetSymbolName(const cppast::cpp_entity& e);
/// Ensure arbitrary string is a valid identifier by replacing invalid characters with "_". "_" will be prepended if string starts with a number.
std::string Sanitize(const std::string& value);
/// Returns true if type is void.
bool IsVoid(const cppast::cpp_type& type);
bool IsVoid(const cppast::cpp_type* type);
/// Returns string padded with _ if value is a common keyword in programming languages.
std::string EnsureNotKeyword(const std::string& value);
/// Return a base type (discards const/pointers/references).
const cppast::cpp_type& GetBaseType(const cppast::cpp_type& type);
/// Return name of underlying type.
std::string GetTypeName(const cppast::cpp_type& type);

class IncludedChecker
{
public:
    IncludedChecker() = default;
    /// Initialize with XMLElement that consists of <include> and <exclude> children tags with wildcards as their values.
    explicit IncludedChecker(const JSONValue& rules);
    /// Initialize with XMLElement that consists of <include> and <exclude> children tags with wildcards as their values.
    void Load(const JSONValue& rules);
    /// Verify string is matched by include rules and not matched by exclude rules.
    bool IsIncluded(const String& value);

protected:
    Vector<std::regex> includes_;
    Vector<std::regex> excludes_;
};
using CppParameters = cppast::detail::iteratable_intrusive_list<cppast::cpp_function_parameter>;
/// Returns a list of parameter types and names as if they were in a function declaration.
std::string ParameterList(const CppParameters& params,
    const std::function<std::string(const cppast::cpp_type&)>& typeToString = nullptr);
/// Returns a list of parameter names separated by comma.
std::string ParameterNameList(const CppParameters& params,
    const std::function<std::string(const cppast::cpp_function_parameter&)>& nameFilter = nullptr);
/// Returns a list of parameter types separated comma. Useful for creating function signatures.
std::string ParameterTypeList(const CppParameters& params,
    const std::function<std::string(const cppast::cpp_type&)>& typeToString = nullptr);
/// Returns true if specified type is an enumeration.
bool IsEnumType(const cppast::cpp_type& type);
/// Returns true if a type is non-builtin value type (not a pointer or reference to a struct/class).
bool IsComplexValueType(const cppast::cpp_type& type);
/// Convert cppast expression into string.
std::string ToString(const cppast::cpp_expression& expression);
/// Get entity which defines user type pointed by `type`. Returns null if not found.
const cppast::cpp_entity* GetEntity(const cppast::cpp_type& type);
/// Returns true if class has a virtual method.
bool HasVirtual(const cppast::cpp_class& cls);
/// Returns true if class has a protected method or variable.
bool HasProtected(const cppast::cpp_class& cls);
/// Returns true if `cls` is subclass of class named `symbol`.
bool IsSubclassOf(const cppast::cpp_class& cls, const std::string& symbol);
/// Returns true if `type` is const.
bool IsConst(const cppast::cpp_type& type);
/// Count number of items in cppast intrusive list.
template<typename T>
int Count(const cppast::detail::iteratable_intrusive_list<T>& list)
{
    int count = 0;
    for (const auto& element : list)
        count++;
    return count;
}
/// Returns true if storage class of entity is static or if entity contains only static members.
bool IsStatic(const cppast::cpp_entity& entity);
/// Converts a builtin type to pinvoke-compatible type.
std::string BuiltinToPInvokeType(const cppast::cpp_type& type);
/// Convert type to pinvoke-compatible type.
std::string ToPInvokeType(const cppast::cpp_type& type, const std::string& default_);
/// Return actual type that is wrapped by supported template types. Like a class type if type is shared pointer.
std::string GetTemplateSubtype(const cppast::cpp_type& type);
}

namespace str
{

std::string& replace_str(std::string& dest, const std::string& find, const std::string& replace);
std::string& replace_str(std::string&& dest, const std::string& find, const std::string& replace);
std::string join(const std::vector<std::string>& collection, const std::string& glue);

}
