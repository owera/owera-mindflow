#include "layout/LayoutEngine.h"

#include "model/Node.h"

#include <algorithm>
#include <vector>

namespace mindflow {

namespace {

using SizeProvider = LayoutEngine::SizeProvider;
using Spacing = LayoutEngine::Spacing;

// Cardinal flow of a level of children (resolved from LayoutDirection).
enum class Flow { Right, Left, Down, Up };

bool horizontal(Flow f) { return f == Flow::Right || f == Flow::Left; }
int flowSign(Flow f) { return (f == Flow::Right || f == Flow::Down) ? +1 : -1; }

// Main axis = direction children grow; cross axis = direction siblings stack.
double mainOf(const QPointF& p, Flow f) { return horizontal(f) ? p.x() : p.y(); }
double crossOf(const QPointF& p, Flow f) { return horizontal(f) ? p.y() : p.x(); }
QPointF makePoint(double main, double cross, Flow f) {
    return horizontal(f) ? QPointF(main, cross) : QPointF(cross, main);
}
double mainSize(const QSizeF& s, Flow f) { return horizontal(f) ? s.width() : s.height(); }
double crossSize(const QSizeF& s, Flow f) { return horizontal(f) ? s.height() : s.width(); }

std::vector<Node*> visibleChildren(const Node* node) {
    std::vector<Node*> out;
    if (node->collapsed)
        return out;
    out.reserve(node->children().size());
    for (const auto& c : node->children())
        out.push_back(c.get());
    return out;
}

Flow flowForRoot(LayoutDirection d) {
    switch (d) {
    case LayoutDirection::LeftDown: return Flow::Left;
    case LayoutDirection::Down: return Flow::Down;
    case LayoutDirection::Up: return Flow::Up;
    default: return Flow::Right; // RightDown and any non-two-sided fallback
    }
}

// Extent of a subtree measured perpendicular to the flow (cross axis).
double breadth(const Node* node, Flow flow, const SizeProvider& sizeOf, const Spacing& sp) {
    const double own = crossSize(sizeOf(node), flow);
    const auto kids = visibleChildren(node);
    if (kids.empty())
        return own;
    double total = 0.0;
    for (Node* c : kids)
        total += breadth(c, flow, sizeOf, sp);
    total += sp.siblingGap * static_cast<double>(kids.size() - 1);
    return std::max(own, total);
}

void assign(Node* node, const QPointF& center, Flow flow, const SizeProvider& sizeOf,
            const Spacing& sp);

void placeChildren(Node* parent, const std::vector<Node*>& kids, Flow flow,
                   const SizeProvider& sizeOf, const Spacing& sp) {
    if (kids.empty())
        return;

    std::vector<double> breadths;
    breadths.reserve(kids.size());
    double total = 0.0;
    for (Node* c : kids) {
        const double b = breadth(c, flow, sizeOf, sp);
        breadths.push_back(b);
        total += b;
    }
    total += sp.siblingGap * static_cast<double>(kids.size() - 1);

    const QPointF parentCenter = parent->layoutPos;
    const double parentMain = mainOf(parentCenter, flow);
    const double halfParentMain = mainSize(sizeOf(parent), flow) / 2.0;

    double cursor = crossOf(parentCenter, flow) - total / 2.0;
    for (size_t i = 0; i < kids.size(); ++i) {
        Node* child = kids[i];
        const double b = breadths[i];
        const double childCross = cursor + b / 2.0;
        const double halfChildMain = mainSize(sizeOf(child), flow) / 2.0;
        const double childMain =
            parentMain + flowSign(flow) * (halfParentMain + sp.depthGap + halfChildMain);

        if (child->hasManualPos)
            assign(child, child->layoutPos, flow, sizeOf, sp);
        else
            assign(child, makePoint(childMain, childCross, flow), flow, sizeOf, sp);

        cursor += b + sp.siblingGap;
    }
}

void assign(Node* node, const QPointF& center, Flow flow, const SizeProvider& sizeOf,
            const Spacing& sp) {
    node->layoutPos = center;
    placeChildren(node, visibleChildren(node), flow, sizeOf, sp);
}

} // namespace

void LayoutEngine::layout(Node* root, const SizeProvider& sizeOf) const {
    if (!root)
        return;

    const QPointF rootCenter = root->hasManualPos ? root->layoutPos : QPointF(0, 0);
    root->layoutPos = rootCenter;

    const LayoutDirection dir = root->layoutDirection;
    Spacing sp = m_spacing;

    const bool twoSided =
        (dir == LayoutDirection::Organic || dir == LayoutDirection::Compact);
    if (dir == LayoutDirection::Compact) {
        sp.depthGap = 44.0;
        sp.siblingGap = 10.0;
    }

    if (!twoSided) {
        const Flow flow = flowForRoot(dir);
        placeChildren(root, visibleChildren(root), flow, sizeOf, sp);
        return;
    }

    // Two-sided: split children between Right and Left, greedily balancing breadth.
    const auto kids = visibleChildren(root);
    std::vector<Node*> right, left;
    double rightB = 0.0, leftB = 0.0;
    for (Node* c : kids) {
        const double b = breadth(c, Flow::Right, sizeOf, sp);
        if (rightB <= leftB) {
            right.push_back(c);
            rightB += b + sp.siblingGap;
        } else {
            left.push_back(c);
            leftB += b + sp.siblingGap;
        }
    }
    placeChildren(root, right, Flow::Right, sizeOf, sp);
    placeChildren(root, left, Flow::Left, sizeOf, sp);
}

} // namespace mindflow
