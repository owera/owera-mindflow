#pragma once

#include "model/NodeStyle.h"

#include <QPointF>
#include <QSizeF>
#include <functional>
#include <vector>

namespace mindflow {

class Node;

// Computes node geometry from the tree and writes it into each Node::layoutPos
// (interpreted as the node's CENTER in scene coordinates). Pure with respect to
// the UI: it asks for each node's rendered size through a callback so it has no
// dependency on QGraphicsItem.
//
// The arrangement is driven by the root's LayoutDirection:
//   Organic            balanced two-sided tidy tree (the default layout)
//   RightDown/LeftDown horizontal tree growing right / left
//   Down/Up            vertical tree growing down / up
//   Compact            two-sided organic with tighter spacing
// (Per-branch mixing of directions is a later refinement; M2 applies one
// direction across the whole tree.)
class LayoutEngine {
public:
    using SizeProvider = std::function<QSizeF(const Node*)>;

    struct Spacing {
        double depthGap = 64.0;   // gap between a node and its child level
        double siblingGap = 22.0; // gap between sibling subtrees
    };

    LayoutEngine() = default;
    explicit LayoutEngine(Spacing spacing) : m_spacing(spacing) {}

    // Lay out the tree rooted at `root`. Nodes with hasManualPos keep their stored
    // center; their subtrees are arranged relative to it.
    void layout(Node* root, const SizeProvider& sizeOf) const;

    const Spacing& spacing() const { return m_spacing; }

private:
    Spacing m_spacing{};
};

} // namespace mindflow
