#include "canvas/ConnectionItem.h"

#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

namespace mindflow {

namespace {
constexpr double kHandleRadius = 6.0;
}

ConnectionItem::ConnectionItem(int connId, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_connId(connId) {
    setFlags(ItemIsSelectable | ItemIsFocusable);
    setZValue(5); // above branches, below nodes
    setAcceptHoverEvents(true);
}

void ConnectionItem::setEndpoints(const QPointF& from, const QPointF& to) {
    prepareGeometryChange();
    m_from = from;
    m_to = to;
    update();
}

void ConnectionItem::setWaypointOffset(const QPointF& offset) {
    prepareGeometryChange();
    m_offset = offset;
    update();
}

void ConnectionItem::setLabel(const QString& label) {
    m_label = label;
    update();
}

QPointF ConnectionItem::controlPoint() const {
    return (m_from + m_to) / 2.0 + m_offset;
}

QPointF ConnectionItem::curveMidpoint() const {
    // Point on a quadratic bezier at t=0.5.
    const QPointF c = controlPoint();
    return 0.25 * m_from + 0.5 * c + 0.25 * m_to;
}

QPainterPath ConnectionItem::buildPath() const {
    QPainterPath path(m_from);
    path.quadTo(controlPoint(), m_to);
    return path;
}

QRectF ConnectionItem::boundingRect() const {
    QRectF r = QRectF(m_from, m_to).normalized();
    r = r.united(QRectF(controlPoint(), QSizeF(1, 1)));
    r = r.adjusted(-20, -20, 20, 20); // room for handle + label
    return r;
}

QPainterPath ConnectionItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(10.0);
    QPainterPath s = stroker.createStroke(buildPath());
    s.addEllipse(curveMidpoint(), kHandleRadius + 2, kHandleRadius + 2);
    return s;
}

void ConnectionItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    const bool sel = isSelected();

    QPen pen(sel ? QColor(0xff, 0xa5, 0x00) : QColor(0x6b, 0x72, 0x80), sel ? 2.4 : 1.8);
    pen.setStyle(Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(buildPath());

    // Waypoint handle.
    const QPointF mid = curveMidpoint();
    painter->setBrush(QColor(Qt::white));
    painter->setPen(QPen(sel ? QColor(0xff, 0xa5, 0x00) : QColor(0x6b, 0x72, 0x80), 1.6));
    painter->drawEllipse(mid, kHandleRadius, kHandleRadius);

    // Label, centered on the curve.
    if (!m_label.isEmpty()) {
        QFontMetricsF fm(painter->font());
        const QRectF tb = fm.boundingRect(m_label).adjusted(-6, -3, 6, 3);
        QRectF box(QPointF(mid.x() - tb.width() / 2, mid.y() - kHandleRadius - tb.height() - 2),
                   tb.size());
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 230));
        painter->drawRoundedRect(box, 4, 4);
        painter->setPen(QColor(0x3a, 0x40, 0x4a));
        painter->drawText(box, Qt::AlignCenter, m_label);
    }
}

void ConnectionItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    const QLineF toHandle(event->pos(), curveMidpoint());
    if (toHandle.length() <= kHandleRadius + 3) {
        m_draggingWaypoint = true;
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void ConnectionItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_draggingWaypoint) {
        // Offset so the curve's midpoint follows the cursor: mid = m_end average +
        // 0.5*offset, so offset = 2*(cursor - endpointAverage).
        const QPointF endpointAvg = (m_from + m_to) / 2.0;
        const QPointF offset = 2.0 * (event->scenePos() - endpointAvg);
        setWaypointOffset(offset);
        emit waypointDragged(m_connId, offset);
        event->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void ConnectionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    const QLineF toHandle(event->pos(), curveMidpoint());
    if (toHandle.length() <= kHandleRadius + 3)
        emit straightenRequested(m_connId); // double-click the handle -> straighten
    else
        emit labelEditRequested(m_connId);  // double-click the line -> edit label
    event->accept();
}

void ConnectionItem::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        emit deleteRequested(m_connId);
        return;
    }
    QGraphicsObject::keyPressEvent(event);
}

} // namespace mindflow
