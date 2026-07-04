#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/BaseVisitable.h"
#include "core/ModuleInstantiation.h"
#include "core/node.h"
#include "geometry/linalg.h"

/*!
   A dimref("name") marker node placed in the model tree.

   Like camera(), it carries no geometry of its own: in every visitor it is
   treated like an empty group (see NodeVisitor.h), so it is invisible in
   preview, render and export and does not disturb measurement. Its purpose is to
   record a named anchor point (via the accumulated transform of its parents) so
   that dimensions (kvóty) can be drawn between two such points and their length
   recomputed from geometry as the model changes.

   The optional kvota= parameter is a convenience: two dimref() nodes sharing the
   same kvota name auto-form a dimension between them, without a separate
   // @dim line.
 */
class DimrefNode : public AbstractNode
{
public:
  VISITABLE();
  DimrefNode(const ModuleInstantiation *mi) : AbstractNode(mi) {}
  std::string toString() const override;
  std::string name() const override;

  // Anchor name, referenced by // @dim A B ... lines.
  std::string dimref_name;
  // Optional auto-pairing dimension name (kvota= sugar). Empty = not set.
  std::string kvota_name;
  // Optional stored size for radial dimensions (// @dim A kind=diameter|radius).
  // The final mesh has no true circle (a cylinder is an $fn-gon), so the value
  // comes from the source here, not from geometry. Negative = unset. May be
  // given as d= (diameter) or r= (radius); either implies the other.
  double diameter{-1.0};
  double radius{-1.0};
};

// A dimref marker together with its accumulated world transform, produced by
// walking the node tree (see collectDimrefs).
struct DimrefInfo {
  Transform3d world;
  const DimrefNode *node;
};

// Walk the node tree from `root`, accumulating TransformNode matrices, and
// return every dimref() marker with its world transform (in tree order).
std::vector<DimrefInfo> collectDimrefs(const std::shared_ptr<const AbstractNode>& root);
