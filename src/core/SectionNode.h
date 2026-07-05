#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/BaseVisitable.h"
#include "core/ModuleInstantiation.h"
#include "core/node.h"
#include "geometry/linalg.h"

/*!
   A section("A") marker node placed in the model tree.

   Like camera() and dimref() it carries no geometry of its own: in every visitor
   it is treated like an empty group (see NodeVisitor.h), so it is invisible in
   preview, render and export and does not disturb measurement.

   Its purpose is to define a cutting plane for a section view in the technical
   drawing. The plane passes through the marker's world origin (from the
   accumulated transform of its parents); its normal is the marker's local +Z
   axis in world space, which is also the viewing direction of the section
   (arrows point +Z). `flip` swaps which half is kept / the view direction.
 */
class SectionNode : public AbstractNode
{
public:
  VISITABLE();
  SectionNode(const ModuleInstantiation *mi) : AbstractNode(mi) {}
  std::string toString() const override;
  std::string name() const override;

  // Section label, shown as "Řez A–A" and next to the cutting line. Empty allowed.
  std::string section_name;
  // Keep the far half by default; flip keeps the near half and reverses the view.
  bool flip{false};
  // Hatch line spacing in drawing-mm for the cut face; negative = auto.
  double hatch{-1.0};
};

// A section marker together with its accumulated world transform, produced by
// walking the node tree (see collectSections).
struct SectionInfo {
  Transform3d world;
  const SectionNode *node;
};

// Walk the node tree from `root`, accumulating TransformNode matrices, and
// return every section() marker with its world transform (in tree order).
std::vector<SectionInfo> collectSections(const std::shared_ptr<const AbstractNode>& root);
