/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "core/ViewNode.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/Arguments.h"
#include "core/Builtins.h"
#include "core/Children.h"
#include "core/Context.h"
#include "core/Expression.h"
#include "core/LocalScope.h"
#include "core/ModuleInstantiation.h"
#include "core/Parameters.h"
#include "core/SourceFile.h"
#include "core/Value.h"
#include "core/module.h"

static std::shared_ptr<AbstractNode> builtin_view(const ModuleInstantiation *inst,
                                                  const std::shared_ptr<const Context>& context)
{
  auto node = std::make_shared<ViewNode>(inst);

  Arguments arguments{inst->arguments, context};
  Parameters parameters = Parameters::parse(std::move(arguments), inst->location(), {"name"}, {});
  node->view_name = parameters.get("name", std::string(""));

  // Whether this view is on the active path. The remaining active path (from this
  // point down) is carried in the special variable $view as a vector of strings;
  // this view matches when $view is non-empty and $view[0] == our name.
  bool active = false;
  std::vector<std::string> rest;
  if (!node->view_name.empty()) {
    if (auto v = context->try_lookup_variable("$view")) {
      if (v->type() == Value::Type::VECTOR) {
        const auto& vec = v->toVector();
        if (vec.size() >= 1 && vec[0].type() == Value::Type::STRING &&
            vec[0].toStrUtf8Wrapper().toString() == node->view_name) {
          active = true;
          for (size_t i = 1; i < vec.size(); ++i) {
            if (vec[i].type() == Value::Type::STRING) {
              rest.push_back(vec[i].toStrUtf8Wrapper().toString());
            }
          }
        }
      }
    }
  }
  node->active = active;

  if (!active) {
    // Not on the active path: instantiate no children, so the whole subtree
    // contributes nothing (an empty group). This is the re-instantiation model.
    return node;
  }

  // On the active path: instantiate children with $view rebound to the tail, so
  // a nested view() selects the next step down the view tree.
  ContextHandle<Context> childContext{Context::create<Context>(context)};
  VectorType restVec(context->session());
  for (const auto& s : rest) restVec.emplace_back(Value(s.c_str()));
  childContext->set_variable("$view", Value(std::move(restVec)));
  return Children(inst->scope, *childContext).instantiate(node);
}

std::string ViewNode::toString() const
{
  std::ostringstream stream;
  stream << "view(name = \"" << this->view_name << "\"";
  if (this->active) stream << ", active = true";
  stream << ")";
  return stream.str();
}

std::string ViewNode::name() const
{
  return "view";
}

// --- AST enumeration (collectViews) ---------------------------------------

// Return the literal string name of a view("...") instantiation, or "" if the
// name is absent or not a literal string (computed names are out of scope for v1).
static std::string literalViewName(const ModuleInstantiation& mi)
{
  const Expression *nameExpr = nullptr;
  for (const auto& arg : mi.arguments) {
    if (!arg) continue;
    if (arg->getName() == "name") {
      nameExpr = arg->getExpr().get();
      break;
    }
    if (arg->getName().empty() && nameExpr == nullptr) {
      nameExpr = arg->getExpr().get();
    }
  }
  if (const auto *lit = dynamic_cast<const Literal *>(nameExpr)) {
    if (lit->isString()) return lit->toString();
  }
  return "";
}

static void collectViewsRecursive(const LocalScope& scope, int depth, int parent,
                                  std::vector<std::string> parentPath,
                                  std::vector<ViewInfo>& out)
{
  for (const auto& mi : scope.moduleInstantiations) {
    if (!mi) continue;
    if (mi->name() == "view") {
      const std::string name = literalViewName(*mi);
      if (!name.empty()) {
        std::vector<std::string> myPath = parentPath;
        myPath.push_back(name);

        ViewInfo info;
        info.name = name;
        info.path = mi->location().fileName();
        info.firstLine = mi->location().firstLine();
        info.lastLine = mi->location().lastLine();
        info.depth = depth;
        info.parent = parent;
        info.fullPath = myPath;
        const int myIndex = static_cast<int>(out.size());
        out.push_back(std::move(info));

        if (mi->scope) {
          collectViewsRecursive(*mi->scope, depth + 1, myIndex, myPath, out);
        }
        continue;
      }
    }
    // Non-view instantiation (or a view with a computed name): descend without
    // changing the enclosing view, so a view() nested under a transform/group
    // is still found with the right parent and path.
    if (mi->scope) {
      collectViewsRecursive(*mi->scope, depth, parent, parentPath, out);
    }
  }
}

std::vector<ViewInfo> collectViews(const SourceFile *file)
{
  std::vector<ViewInfo> views;
  if (file && file->scope) {
    collectViewsRecursive(*file->scope, 0, -1, {}, views);
  }
  return views;
}

void register_builtin_view()
{
  Builtins::init("view", new BuiltinModule(builtin_view),
                 {
                   "view(name = \"A\") { ... }  // switchable model view / state",
                 });
}
