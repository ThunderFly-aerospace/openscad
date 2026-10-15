#pragma once

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "core/BaseVisitable.h"
#include "core/CSGNode.h"
#include "core/ModuleInstantiation.h"
#include "core/NodeVisitor.h"
#include "core/enums.h"
#include "core/node.h"
#include "geometry/Geometry.h"

class CSGNode;
class GeometryEvaluator;
class Tree;

class CSGTreeEvaluator : public NodeVisitor
{
public:
  CSGTreeEvaluator(const Tree& tree, GeometryEvaluator *geomevaluator = nullptr)
    : tree(tree), geomevaluator(geomevaluator)
  {
  }

  // slicer_modifier() regions: by default buildCSGTree() also builds a colored
  // model * region term per region (getSlicerRegionNodes); with false the
  // regions are left out entirely.
  void setShowSlicerModifiers(bool show) { this->showSlicerModifiers = show; }

  Response visit(State& state, const AbstractNode& node) override;
  Response visit(State& state, const AbstractIntersectionNode& node) override;
  Response visit(State& state, const AbstractPolyNode& node) override;
  Response visit(State& state, const ListNode& node) override;
  Response visit(State& state, const CsgOpNode& node) override;
  Response visit(State& state, const TransformNode& node) override;
  Response visit(State& state, const ColorNode& node) override;
  Response visit(State& state, const RenderNode& node) override;
  Response visit(State& state, const BendNode& node) override;
  Response visit(State& state, const SlicerModifierNode& node) override;
  Response visit(State& state, const CgalAdvNode& node) override;

  std::shared_ptr<CSGNode> buildCSGTree(const AbstractNode& node);

  [[nodiscard]] const std::shared_ptr<CSGNode>& getRootNode() const { return this->rootNode; }
  [[nodiscard]] const std::vector<std::shared_ptr<CSGNode>>& getHighlightNodes() const
  {
    return this->highlightNodes;
  }
  [[nodiscard]] const std::vector<std::shared_ptr<CSGNode>>& getBackgroundNodes() const
  {
    return this->backgroundNodes;
  }
  // model * region term per slicer_modifier(), recolored with the region color,
  // in source order (later regions are drawn on top).
  [[nodiscard]] const std::vector<std::shared_ptr<CSGNode>>& getSlicerRegionNodes() const
  {
    return this->slicerRegionNodes;
  }

private:
  void addToParent(const State& state, const AbstractNode& node);
  void applyToChildren(State& state, const AbstractNode& node, OpenSCADOperator op);
  std::shared_ptr<CSGNode> evaluateCSGNodeFromGeometry(State& state,
                                                       const std::shared_ptr<const Geometry>& geom,
                                                       const ModuleInstantiation *modinst,
                                                       const AbstractNode& node);
  void applyBackgroundAndHighlight(State& state, const AbstractNode& node);

  using ChildList = std::list<std::shared_ptr<const AbstractNode>>;
  std::map<int, ChildList> visitedchildren;

protected:
  const Tree& tree;
  GeometryEvaluator *geomevaluator;
  bool showSlicerModifiers{true};
  // slicer_modifier() region terms (union of the node's children) with their
  // display color, applied to the root term at the end of buildCSGTree().
  struct SlicerRegion {
    std::shared_ptr<CSGNode> term;
    Color4f color;
  };
  std::vector<SlicerRegion> slicerRegions;
  std::vector<std::shared_ptr<CSGNode>> slicerRegionNodes;
  std::shared_ptr<CSGNode> rootNode;
  std::vector<std::shared_ptr<CSGNode>> highlightNodes;
  std::vector<std::shared_ptr<CSGNode>> backgroundNodes;
  std::map<int, std::shared_ptr<CSGNode>> stored_term;  // The term evaluated from each node index
};
