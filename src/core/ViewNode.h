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

   Fáze 17: a view may additionally declare which batch exports it is a default
   for, with three independent optional flags (all false by default):

     view(name = "pcb", camera_export = false, drawings_export = true,
          model_export = true) { ... }

   The CLI --default-model-views then processes exactly the views marked for the
   export that is running (see ViewExportKind / viewIsDefaultFor).
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
  bool camera_export{false};
  bool drawings_export{false};
  bool model_export{false};
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
  // Fáze 17: the export-default flags, as literal true/false in the source (a
  // computed value reads as false — the AST walk does not evaluate expressions).
  bool cameraExport{false};
  bool drawingsExport{false};
  bool modelExport{false};
};

// The batch export a --default-model-views run is performing, i.e. which of the
// three view() flags selects the views to process.
enum class ViewExportKind {
  Cameras,   // --export-cameras  -> camera_export
  Drawings,  // --export-drawing  -> drawings_export
  Model,     // -o <file>         -> model_export
};

// Is this view marked as a default for the given export kind?
bool viewIsDefaultFor(const ViewInfo& info, ViewExportKind kind);

class SourceFile;

// Walk the parsed AST of `file` (following include<> content) and return every
// literal view("name") block with its source range and nesting, in source order.
// Non-literal names (computed) are skipped (out of scope for v1).
//
// The walk also enters the bodies of called user modules (resolved like the runtime
// does: enclosing scopes innermost-first, then the use<>d files) and both branches of
// if/else, so a view() defined inside a module is found too. A view name can therefore
// appear more than once — several blocks may carry the same name, and that is legal
// (they are all active together). Consumers that map a name to *one* output file want
// uniqueViewPaths(); consumers that map source lines to views (annotation scoping)
// need every occurrence.
std::vector<ViewInfo> collectViews(const SourceFile *file);

// First occurrence of each distinct view path, in source order.
std::vector<ViewInfo> uniqueViewPaths(const std::vector<ViewInfo>& views);
