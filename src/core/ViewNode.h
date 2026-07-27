#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/BaseVisitable.h"
#include "core/ModuleInstantiation.h"
#include "core/node.h"

/*!
   A view("name") { ... } marker node placed in the model tree.

   Unlike camera()/section()/dimref(), a view() is a *transparent container*: it
   carries no geometry of its own but it does pass its children through (in every
   visitor it is treated like a plain group, see NodeVisitor.h). It names a
   switchable "view" (a selection / state of the model).

   Semantics (see ROADMAP Fáze 15): exactly one view path is active at a time,
   carried in the special variable $view (a vector of strings = the remaining
   active path from this point down). A view() instantiates its children only
   when it is on the active path — i.e. $view is non-empty and $view[0] equals
   this view's name; the children are then instantiated with $view rebound to
   the tail $view[1:], so nesting selects a path down the view tree. When the
   view is not on the active path it instantiates no children, so it (and its
   whole subtree) contributes nothing to preview/render/export — this is the
   "re-instantiation" model, not a render-time visibility filter.

   The default active path is empty ($view == []), so at the root every view()
   is inactive and only geometry outside any view() block ("common") shows —
   which is exactly the current behaviour of a model without any view().

   `active` records whether this instance was on the active path when it was
   instantiated, purely for toString()/debugging and dump/echo tests.
 */
class ViewNode : public AbstractNode
{
public:
  VISITABLE();
  ViewNode(const ModuleInstantiation *mi) : AbstractNode(mi) {}
  std::string toString() const override;
  std::string name() const override;

  std::string view_name;
  bool active{false};
};

// A view marker together with its source location, produced by walking the AST
// (not the instantiated node tree) so that inactive and nested views — and views
// pulled in via include<> — are all discovered. See collectViews().
struct ViewInfo {
  std::string name;              // literal name from view("...")
  std::string path;              // source file the view() call is in (Location::path)
  int firstLine{-1};             // 1-based first line of the view() block
  int lastLine{-1};              // 1-based last line (closing brace) of the block
  int depth{0};                  // nesting depth (0 = top level)
  int parent{-1};                // index of the enclosing view in the returned vector, or -1
  std::vector<std::string> fullPath;  // names from the tree root down to and incl. this view
};

class SourceFile;

// Walk the parsed AST of `file` (following include<> content) and return every
// literal view("name") block with its source range and nesting, in source order.
// Non-literal names (computed) are skipped (out of scope for v1).
std::vector<ViewInfo> collectViews(const SourceFile *file);
