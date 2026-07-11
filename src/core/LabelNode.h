#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/BaseVisitable.h"
#include "core/ModuleInstantiation.h"
#include "core/node.h"
#include "geometry/linalg.h"

/*!
   A label("name", "text") marker node placed in the model tree.

   Like camera() and dimref(), it carries no geometry of its own: in every
   visitor it is treated like an empty group (see NodeVisitor.h), so it is
   invisible in preview, render and export and does not disturb measurement. Its
   purpose is to record a named anchor point (via the accumulated transform of
   its parents) together with a piece of annotation text, so that a
   // @label name proj=... line can draw that text at a chosen 2D position in a
   drawing view or camera snapshot, with a leader line back to the projected
   anchor point. The anchor rides the model's transforms, so the callout tracks
   the geometry as the model changes.

   The text is authored here in 3D (it travels with the transforms conceptually);
   the // @label comment picks the projection and the text placement, and may
   override the text with text="...".
 */
class LabelNode : public AbstractNode
{
public:
  VISITABLE();
  LabelNode(const ModuleInstantiation *mi) : AbstractNode(mi) {}
  std::string toString() const override;
  std::string name() const override;

  // Anchor name, referenced by // @label name ... lines.
  std::string label_name;
  // Annotation text shown at the label. Defaults to label_name when omitted.
  std::string text;
};

// A label marker together with its accumulated world transform, produced by
// walking the node tree (see collectLabels).
struct LabelInfo {
  Transform3d world;
  const LabelNode *node;
};

// Walk the node tree from `root`, accumulating TransformNode matrices, and
// return every label() marker with its world transform (in tree order).
std::vector<LabelInfo> collectLabels(const std::shared_ptr<const AbstractNode>& root);
