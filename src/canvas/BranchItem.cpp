#include "canvas/BranchItem.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>

namespace mindflow {

BranchItem::BranchItem(QGraphicsItem* parent) : QGraphicsItem(parent) {
    setZValue(0); // behind nodes
    setCacheMode(DeviceCoordinateCache);
}

void BranchItem::setEndpoints(const QPointF& from, const QPointF& to) {
    prepareGeometryChange();
    m_from = from;
    m_to = to;
    update();
}

QPainterPath BranchItem::buildPath() const {
    QPainterPath path(m_from);
    if (m_connector == ConnectorStyle::Orthogonal) {
        const double midX = (m_from.x() + m_to.x()) / 2.0;
        path.lineTo(midX, m_from.y());
        path.lineTo(midX, m_to.y());
        path.lineTo(m_to);
    } else {
        // Cubic bezier with horizontal control handles -> smooth organic sweep.
        const double dx = (m_to.x() - m_from.x()) * 0.5;
        const QPointF c1(m_from.x() + dx, m_from.y());
        const QPointF c2(m_to.x() - dx, m_to.y());
        path.cubicTo(c1, c2, m_to);
    }
    return path;
}

QRectF BranchItem::boundingRect() const {
    const double pad = m_width + 2.0;
    return QRectF(m_from, m_to).normalized().adjusted(-pad, -pad, pad, pad);
}

QPainterPath BranchItem::shape() const {
    QPainterPathStroker stroker;
    stroker.setWidth(m_width + 6.0);
    return stroker.createStroke(buildPath());
}

void BranchItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(m_color, m_width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(buildPath());
}

} // namespace mindflow
