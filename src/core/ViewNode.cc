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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string/join.hpp>

#include "core/Arguments.h"
#include "core/Builtins.h"
#include "core/Children.h"
#include "core/Context.h"
#include "core/Expression.h"
#include "core/LocalScope.h"
#include "core/ModuleInstantiation.h"
#include "core/Parameters.h"
#include "core/SourceFile.h"
#include "core/SourceFileCache.h"
#include "core/UserModule.h"
#include "core/Value.h"
#include "core/module.h"

static std::shared_ptr<AbstractNode> builtin_view(const ModuleInstantiation *inst,
                                                  const std::shared_ptr<const Context>& context)
{
  auto node = std::make_shared<ViewNode>(inst);

  Arguments arguments{inst->arguments, context};
  Parameters parameters = Parameters::parse(std::move(arguments), inst->location(), {"name"},
                                           {"camera_export", "drawings_export", "model_export"});
  node->view_name = parameters.get("name", std::string(""));

  // Fáze 17: export-default flags, all false unless given as a boolean.
  const auto flag = [&parameters](const char *name) {
    return parameters[name].type() == Value::Type::BOOL && parameters[name].toBool();
  };
  node->camera_export = flag("camera_export");
  node->drawings_export = flag("drawings_export");
  node->model_export = flag("model_export");

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
  if (this->camera_export) stream << ", camera_export = true";
  if (this->drawings_export) stream << ", drawings_export = true";
  if (this->model_export) stream << ", model_export = true";
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

// Value of a named boolean argument written as a literal, e.g. camera_export = true.
// The AST walk does not evaluate expressions, so a computed value reads as false.
static bool literalBoolArg(const ModuleInstantiation& mi, const std::string& argName)
{
  for (const auto& arg : mi.arguments) {
    if (!arg || arg->getName() != argName) continue;
    if (const auto *lit = dynamic_cast<const Literal *>(arg->getExpr().get())) {
      return lit->isBool() && lit->toBool();
    }
    return false;
  }
  return false;
}

bool viewIsDefaultFor(const ViewInfo& info, ViewExportKind kind)
{
  switch (kind) {
  case ViewExportKind::Cameras: return info.cameraExport;
  case ViewExportKind::Drawings: return info.drawingsExport;
  case ViewExportKind::Model: return info.modelExport;
  }
  return false;
}

// State carried down the AST walk so that a view() inside a *user module* is found
// as well: the module body is not part of the call site's scope, so the walk has to
// resolve the module name and descend into its definition. The resolution mirrors
// the runtime lookup in FileContext::lookup_local_module (enclosing scopes first,
// innermost wins, then the use<>d files of the current file).
struct ViewWalk {
  const SourceFile *file{nullptr};                 // file whose use<>d libraries apply
  std::vector<const LocalScope *> scopes;          // enclosing scopes, innermost last
  std::vector<const UserModule *> moduleStack;     // guards against recursive modules
  std::set<std::string> *walkedBodies{nullptr};    // module bodies already walked, per view path
};

struct FoundModule {
  const UserModule *module{nullptr};
  const SourceFile *file{nullptr};  // set when found in a use<>d file
  int scopeDepth{-1};               // index in walk.scopes where it was defined, -1 = used file
};

static FoundModule lookupUserModule(const ViewWalk& walk, const std::string& name)
{
  for (int i = static_cast<int>(walk.scopes.size()) - 1; i >= 0; --i) {
    const auto& modules = walk.scopes[i]->getUserModules();
    const auto found = modules.find(name);
    if (found != modules.end()) return {found->second.get(), nullptr, i};
  }
  if (walk.file) {
    for (const auto& lib : walk.file->usedlibs) {
      // Null if the library failed to compile or was not found.
      const SourceFile *used = SourceFileCache::instance()->lookup(lib);
      if (!used || !used->scope) continue;
      const auto& modules = used->scope->getUserModules();
      const auto found = modules.find(name);
      if (found != modules.end()) return {found->second.get(), used, -1};
    }
  }
  return {};
}

static void collectViewsRecursive(const LocalScope& scope, int depth, int parent,
                                  std::vector<std::string> parentPath, ViewWalk& walk,
                                  std::vector<ViewInfo>& out);

// Descend into the body of a called user module, with the scope chain of its
// *definition* site (a module body sees where it was defined, not where it is called).
static void collectViewsInModule(const FoundModule& found, int depth, int parent,
                                 const std::vector<std::string>& parentPath, const ViewWalk& walk,
                                 std::vector<ViewInfo>& out)
{
  if (!found.module || !found.module->body) return;
  // A module called from many places would otherwise have its body walked once per
  // call site, yielding duplicates (and, with nesting, an exponential walk). The
  // views found are identical for the same enclosing view path, so once is enough.
  const std::string memo = std::to_string(reinterpret_cast<uintptr_t>(found.module)) + "@" +
                           boost::algorithm::join(parentPath, "/");
  if (walk.walkedBodies && !walk.walkedBodies->insert(memo).second) return;
  if (std::find(walk.moduleStack.begin(), walk.moduleStack.end(), found.module) !=
      walk.moduleStack.end()) {
    return;  // recursive module: stop
  }

  ViewWalk sub;
  sub.file = found.file ? found.file : walk.file;
  sub.walkedBodies = walk.walkedBodies;
  sub.moduleStack = walk.moduleStack;
  sub.moduleStack.push_back(found.module);
  if (found.scopeDepth >= 0) {
    sub.scopes.assign(walk.scopes.begin(), walk.scopes.begin() + found.scopeDepth + 1);
  } else if (sub.file && sub.file->scope) {
    sub.scopes.push_back(sub.file->scope.get());  // defined in a use<>d file
  }
  collectViewsRecursive(*found.module->body, depth, parent, parentPath, sub, out);
}

static void collectViewsRecursive(const LocalScope& scope, int depth, int parent,
                                  std::vector<std::string> parentPath, ViewWalk& walk,
                                  std::vector<ViewInfo>& out)
{
  walk.scopes.push_back(&scope);
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
        info.cameraExport = literalBoolArg(*mi, "camera_export");
        info.drawingsExport = literalBoolArg(*mi, "drawings_export");
        info.modelExport = literalBoolArg(*mi, "model_export");
        const int myIndex = static_cast<int>(out.size());
        out.push_back(std::move(info));

        if (mi->scope) {
          collectViewsRecursive(*mi->scope, depth + 1, myIndex, myPath, walk, out);
        }
        continue;
      }
    }
    // Non-view instantiation (or a view with a computed name): descend without
    // changing the enclosing view, so a view() nested under a transform/group
    // is still found with the right parent and path.
    if (mi->scope) {
      collectViewsRecursive(*mi->scope, depth, parent, parentPath, walk, out);
    }
    // The else branch of if/else hangs off the instantiation separately, so a view()
    // in it needs walking explicitly. Both branches are reported: the walk is static,
    // it enumerates every view the model can select, not the live one.
    if (const auto *ifelse = dynamic_cast<const IfElseModuleInstantiation *>(mi.get())) {
      if (const auto elseScope = ifelse->getElseScope()) {
        collectViewsRecursive(*elseScope, depth, parent, parentPath, walk, out);
      }
    }
    // A call of a user module: its body is not in this scope, so resolve the name and
    // walk the definition. This is what makes view()s inside modules visible to
    // --list-model-views / --all-model-views / --default-model-views and the GUI switcher.
    collectViewsInModule(lookupUserModule(walk, mi->name()), depth, parent, parentPath, walk, out);
  }
  walk.scopes.pop_back();
}

std::vector<ViewInfo> collectViews(const SourceFile *file)
{
  std::vector<ViewInfo> views;
  if (file && file->scope) {
    std::set<std::string> walkedBodies;
    ViewWalk walk;
    walk.file = file;
    walk.walkedBodies = &walkedBodies;
    collectViewsRecursive(*file->scope, 0, -1, {}, walk, views);
  }
  return views;
}

std::vector<ViewInfo> uniqueViewPaths(const std::vector<ViewInfo>& views)
{
  std::vector<ViewInfo> unique;
  std::set<std::vector<std::string>> seen;
  for (const auto& v : views) {
    if (seen.insert(v.fullPath).second) unique.push_back(v);
  }
  return unique;
}

void register_builtin_view()
{
  Builtins::init("view", new BuiltinModule(builtin_view),
                 {
                   "view(name = \"A\") { ... }  // switchable model view / state",
                 });
}
