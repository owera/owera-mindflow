#pragma once

#include <QGraphicsObject>
#include <QPixmap>
#include <QRectF>

namespace mindflow {

class Node;

// Visual representation of one Node on the canvas. A QGraphicsObject so it can be
// animated (QPropertyAnimation on pos) and emit signals the MindMapView wires to
// the Document. layoutPos from the model is the node's CENTER; this item maps that
// to its top-left via setPos.
class NodeItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit NodeItem(Node* node, QGraphicsItem* parent = nullptr);

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    Node* node() const { return m_node; }
    // Stable node id cached at construction. Safe to read even if the underlying
    // Node was freed (e.g. during reset/open) before the scene is rebuilt.
    int nodeId() const { return m_id; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

    // Size of the drawn box (used by the layout engine via a size provider).
    QSizeF size() const;
    // Center point in scene coordinates (where branches attach).
    QPointF sceneCenter() const;

    // Refresh cached geometry/text after the model node changed.
    void refresh();
    // Re-position from the model's layoutPos (center). Called after a layout pass.
    void syncPositionFromModel();
    // Place the item's top-left directly (used while animating). No move commit.
    void setTopLeft(const QPointF& topLeft);

    // Tell the item where its children are so the fold knob sits on that edge.
    void setFoldHint(bool hasChildren, const QPointF& childrenSceneCenter);

    void beginEditing();
    bool isEditing() const;
    void commitEditing(); // commit the current inline edit without grabbing focus
    void cancelEditing(); // discard the current inline edit, restoring the old text

signals:
    void textCommitted(mindflow::Node* node, const QString& text);
    void positionChangedByUser(mindflow::Node* node, const QPointF& sceneCenter);
    void addChildRequested(mindflow::Node* node);
    void addSiblingRequested(mindflow::Node* node);
    void deleteRequested(mindflow::Node* node);
    void toggleCollapseRequested(mindflow::Node* node);
    void toggleTaskRequested(mindflow::Node* node);
    void navigateRequested(mindflow::Node* from, int key); // arrow-key navigation
    void geometryChanged(); // position moved (e.g. during a drag)

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    QRectF foldKnobRect() const; // empty if no knob shown
    QPainterPath shapePath(const QRectF& r) const;
    void finishEditing(bool commit, bool refocus);
    void relayoutContent();      // recompute internal element rects + m_size

    Node* m_node;
    int m_id; // cached node id (see nodeId())
    QSizeF m_size;
    class QGraphicsTextItem* m_editor = nullptr;
    bool m_suppressMoveCommit = false;
    bool m_hasChildren = false;
    int m_foldSide = +1; // +1 knob on right edge, -1 on left edge

    // Cached content layout (in item coords, recomputed by relayoutContent()).
    QPixmap m_thumb;            // decoded image thumbnail
    QRectF m_imageRect;
    QRectF m_checkboxRect;      // empty if not a task
    QRectF m_textArea;          // where sticker + text are drawn
    QRectF m_tagsRect;          // where tag chips are drawn
    int m_taskDone = 0;         // subtree task progress
    int m_taskTotal = 0;
};

} // namespace mindflow
