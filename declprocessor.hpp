#pragma once
#include <functional>

#include "vvvclanghelper.hpp"


using decl_processor_t = std::function<void(const clang::Decl*)>;

void visit_decls(int argc, const char** argv, const decl_processor_t& f);
