#pragma once

#include <QGraphicsObject>

namespace mindflow {

// Visual for a cross-connection: a dashed curved line between two node centers
// with a draggable waypoint handle (curve), double-click-to-straighten, and an
// editable label. Endpoints are set by the view after each layout pass. Emits
// signals the view translates into undoable Document operations.
class ConnectionItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit ConnectionItem(int connId, QGraphicsItem* parent = nullptr);

    enum { Type = UserType + 3 };
    int type() const override { return Type; }

    int connectionId() const { return m_connId; }

    // from/to are scene-space node centers; offset displaces the control point.
    void setEndpoints(const QPointF& from, const QPointF& to);
    void setWaypointOffset(const QPointF& offset);
    void setLabel(const QString& label);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

signals:
    void waypointDragged(int connId, const QPointF& offset);
    void straightenRequested(int connId);
    void labelEditRequested(int connId);
    void deleteRequested(int connId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPointF controlPoint() const; // midpoint + offset
    QPointF curveMidpoint() const; // point ON the curve (handle position)
    QPainterPath buildPath() const;

    int m_connId;
    QPointF m_from;
    QPointF m_to;
    QPointF m_offset;
    QString m_label;
    bool m_draggingWaypoint = false;
};

} // namespace mindflow
