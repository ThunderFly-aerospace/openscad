#pragma once

#include "core/BaseVisitable.h"
#include "core/State.h"
#include "core/node.h"

class State;

class NodeVisitor : public BaseVisitor,
                    public Visitor<class AbstractNode>,
                    public Visitor<class AbstractIntersectionNode>,
                    public Visitor<class AbstractPolyNode>,
                    public Visitor<class ListNode>,
                    public Visitor<class GroupNode>,
                    public Visitor<class RootNode>,
                    public Visitor<class LeafNode>,
                    public Visitor<class CgalAdvNode>,
                    public Visitor<class CsgOpNode>,
                    public Visitor<class LinearExtrudeNode>,
                    public Visitor<class RotateExtrudeNode>,
                    public Visitor<class RoofNode>,
                    public Visitor<class ImportNode>,
                    public Visitor<class TextNode>,
                    public Visitor<class ProjectionNode>,
                    public Visitor<class RenderNode>,
                    public Visitor<class SurfaceNode>,
                    public Visitor<class TransformNode>,
                    public Visitor<class ColorNode>,
                    public Visitor<class CameraNode>,
                    public Visitor<class DimrefNode>,
                    public Visitor<class SectionNode>,
                    public Visitor<class OffsetNode>
{
public:
  NodeVisitor() = default;

  Response traverse(const AbstractNode& node, const State& state = NodeVisitor::nullstate);

  Response visit(State& state, const AbstractNode& node) override = 0;
  Response visit(State& state, const AbstractIntersectionNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const AbstractPolyNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const ListNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }

  Response visit(State& state, const GroupNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const RootNode& node) override
  {
    return visit(state, (const GroupNode&)node);
  }
  Response visit(State& state, const LeafNode& node) override
  {
    return visit(state, (const AbstractPolyNode&)node);
  }
  Response visit(State& state, const CgalAdvNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const CsgOpNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const LinearExtrudeNode& node) override
  {
    return visit(state, (const AbstractPolyNode&)node);
  }
  Response visit(State& state, const RotateExtrudeNode& node) override
  {
    return visit(state, (const AbstractPolyNode&)node);
  }
  Response visit(State& state, const RoofNode& node) override
  {
    return visit(state, (const AbstractPolyNode&)node);
  }
  Response visit(State& state, const ImportNode& node) override
  {
    return visit(state, (const LeafNode&)node);
  }
  Response visit(State& state, const TextNode& node) override
  {
    return visit(state, (const AbstractPolyNode&)node);
  }
  Response visit(State& state, const ProjectionNode& node) override
  {
    return visit(state, (const AbstractPolyNode&)node);
  }
  Response visit(State& state, const RenderNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const SurfaceNode& node) override
  {
    return visit(state, (const LeafNode&)node);
  }
  Response visit(State& state, const TransformNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const ColorNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  // camera() is a pose marker without geometry: treat it like an empty group
  // so it stays invisible in preview/render/export and does not break measurement.
  Response visit(State& state, const CameraNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  // dimref() is a named anchor marker without geometry: treat it like an empty
  // group so it stays invisible in preview/render/export and does not break
  // measurement.
  Response visit(State& state, const DimrefNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  // section() is a cutting-plane marker without geometry: treat it like an empty
  // group so it stays invisible in preview/render/export and does not break
  // measurement.
  Response visit(State& state, const SectionNode& node) override
  {
    return visit(state, (const AbstractNode&)node);
  }
  Response visit(State& state, const OffsetNode& node) override
  {
    return visit(state, (const AbstractPolyNode&)node);
  }
  // Add visit() methods for new visitable subtypes of AbstractNode here

private:
  static State nullstate;
};
