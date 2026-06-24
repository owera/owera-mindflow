#pragma once

#include "model/NodeStyle.h"

#include <QGraphicsItem>

namespace mindflow {

// A connector drawn from a parent node to a child node. Lives in scene
// coordinates; the MindMapView sets its endpoints after each layout pass. Rounded
// mode draws a cubic bezier (the signature organic look); Orthogonal draws elbows.
class BranchItem : public QGraphicsItem {
public:
    explicit BranchItem(QGraphicsItem* parent = nullptr);

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    void setEndpoints(const QPointF& from, const QPointF& to);
    void setConnector(ConnectorStyle style) { m_connector = style; }
    void setColor(const QColor& color) { m_color = color; }
    void setWidth(double w) { m_width = w; }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget) override;

private:
    QPainterPath buildPath() const;

    QPointF m_from;
    QPointF m_to;
    ConnectorStyle m_connector = ConnectorStyle::Rounded;
    QColor m_color = QColor(0x2d, 0x6c, 0xdf);
    double m_width = 2.5;
};

} // namespace mindflow
