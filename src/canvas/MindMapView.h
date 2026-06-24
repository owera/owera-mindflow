#pragma once

#include "layout/LayoutEngine.h"

#include "model/NodeStyle.h"

#include <QGraphicsView>
#include <QHash>
#include <QList>
#include <QPointF>

#include <functional>

class QVariantAnimation;

namespace mindflow {

class Document;
class Node;
class NodeItem;
class BranchItem;
class ConnectionItem;

// The Mind Map canvas. Owns a QGraphicsScene, mirrors the Document tree into
// NodeItem/BranchItem objects, runs the LayoutEngine, and forwards user gestures
// (add/edit/delete/move) back to the Document as undoable operations.
class MindMapView : public QGraphicsView {
    Q_OBJECT
public:
    explicit MindMapView(QWidget* parent = nullptr);

    void setDocument(Document* doc);
    Document* document() const { return m_doc; }

signals:
    void selectionChanged(mindflow::Node* node); // nullptr when nothing selected

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void zoomToFit();
    void addChildToSelection();
    void editSelection();
    void deleteSelection();
    void toggleCollapseSelection();
    void detachSelection();
    void setSelectionLayoutDirection(mindflow::LayoutDirection dir);
    // Spotlight nodes carrying `tag` (dim the rest); empty clears the highlight.
    void setTagHighlight(const QString& tag);
    void connectSelection();          // cross-connect the two selected nodes
    void setConnectionsVisible(bool on);
    bool connectionsVisible() const { return m_connectionsVisible; }
    void toggleFocusOnSelection();    // focus selected subtree (toggle off if same)
    void clearFocus();
    void searchAndSelect(const QString& query); // select + center matches
    void selectNode(mindflow::Node* node);      // select + center one node

protected:
    void showEvent(QShowEvent* event) override;
    // Intercept Tab/Backtab key presses in event() — before QWidget's focus-traversal
    // handling consumes them — so Tab reaches our keyboard commands (add child).
    bool event(QEvent* event) override;
    // Real keystrokes are delivered to the viewport (the view's focus proxy), so we
    // filter its key presses too; both routes funnel into handleCommandKey().
    bool eventFilter(QObject* obj, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override; // command keys (Tab/Enter/…)
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuild();                 // full re-mirror of the model into items
    void relayout(bool animate = false); // run layout + (animate) reposition + edges
    void updateConnections();       // refresh branch endpoints + fold hints
    QHash<NodeItem*, QPointF> computeTargets() const; // target top-left per item
    void wireItem(NodeItem* item);
    void wireConnection(ConnectionItem* item);
    void applyEmphasis();     // re-apply tag-highlight + focus dimming to all items
    bool inFocusSubtree(Node* node) const;
    NodeItem* itemFor(Node* node) const { return m_nodeItems.value(node, nullptr); }
    NodeItem* selectedItem() const; // the single selected NodeItem, or nullptr
    NodeItem* editingItem() const;  // the NodeItem with an open inline editor, or null
    // Handle a canvas command key (Tab/Enter/F2/Delete/arrows). Returns true if it
    // was consumed. Targets the node being edited, else the selected node.
    bool handleCommandKey(QKeyEvent* event);
    QList<Node*> selectedNodes() const;
    void selectAndMaybeEdit(Node* node, bool edit);
    Node* selectedNode() const;

    // model-signal handlers
    void onStructureChanged();
    void onNodeChanged(Node* node);

    // interaction handlers from NodeItem
    void handleAddChild(Node* parent);
    void handleAddSibling(Node* node);
    void handleDelete(Node* node);
    void handleTextCommitted(Node* node, const QString& text);
    void handlePositionChanged(Node* node, const QPointF& sceneCenter);
    void handleToggleCollapse(Node* node);
    void handleToggleTask(Node* node);
    // Move selection to the nearest node in the arrow-key direction (any layout).
    void handleNavigate(Node* from, int key);

    void applyZoom(double factor);

    Document* m_doc = nullptr;
    QGraphicsScene* m_scene = nullptr;
    LayoutEngine m_layout;
    QHash<Node*, NodeItem*> m_nodeItems;
    QList<BranchItem*> m_branches;
    QList<ConnectionItem*> m_connectionItems;
    bool m_connectionsVisible = true;
    int m_focusNodeId = -1;            // focused subtree root (-1 = no focus)
    double m_zoom = 1.0;
    bool m_panning = false;
    bool m_initialFitDone = false;
    QPoint m_lastPanPoint;
    QString m_highlightTag; // active tag-highlight filter ("" = none)

    // Animation of layout transitions.
    QVariantAnimation* m_anim = nullptr;
    QHash<NodeItem*, QPointF> m_animStart;
    QHash<NodeItem*, QPointF> m_animTarget;
};

} // namespace mindflow
