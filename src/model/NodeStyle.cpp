#include "model/NodeStyle.h"

#include <QMetaEnum>

namespace mindflow {

QString toString(NodeShape s) {
    switch (s) {
    case NodeShape::Line: return QStringLiteral("line");
    case NodeShape::Embedded: return QStringLiteral("embedded");
    case NodeShape::Rectangle: return QStringLiteral("rectangle");
    case NodeShape::Rounded: return QStringLiteral("rounded");
    case NodeShape::Pill: return QStringLiteral("pill");
    case NodeShape::Cloud: return QStringLiteral("cloud");
    case NodeShape::Hexagon: return QStringLiteral("hexagon");
    case NodeShape::Octagon: return QStringLiteral("octagon");
    }
    return QStringLiteral("rounded");
}

NodeShape nodeShapeFromString(const QString& v) {
    if (v == QLatin1String("line")) return NodeShape::Line;
    if (v == QLatin1String("embedded")) return NodeShape::Embedded;
    if (v == QLatin1String("rectangle")) return NodeShape::Rectangle;
    if (v == QLatin1String("pill")) return NodeShape::Pill;
    if (v == QLatin1String("cloud")) return NodeShape::Cloud;
    if (v == QLatin1String("hexagon")) return NodeShape::Hexagon;
    if (v == QLatin1String("octagon")) return NodeShape::Octagon;
    return NodeShape::Rounded;
}

QString toString(LayoutDirection d) {
    switch (d) {
    case LayoutDirection::Organic: return QStringLiteral("organic");
    case LayoutDirection::RightDown: return QStringLiteral("right");
    case LayoutDirection::LeftDown: return QStringLiteral("left");
    case LayoutDirection::Down: return QStringLiteral("down");
    case LayoutDirection::Up: return QStringLiteral("up");
    case LayoutDirection::Compact: return QStringLiteral("compact");
    }
    return QStringLiteral("organic");
}

LayoutDirection layoutDirectionFromString(const QString& v) {
    if (v == QLatin1String("right")) return LayoutDirection::RightDown;
    if (v == QLatin1String("left")) return LayoutDirection::LeftDown;
    if (v == QLatin1String("down")) return LayoutDirection::Down;
    if (v == QLatin1String("up")) return LayoutDirection::Up;
    if (v == QLatin1String("compact")) return LayoutDirection::Compact;
    return LayoutDirection::Organic;
}

QString toString(ConnectorStyle c) {
    return c == ConnectorStyle::Orthogonal ? QStringLiteral("orthogonal")
                                           : QStringLiteral("rounded");
}

ConnectorStyle connectorStyleFromString(const QString& v) {
    return v == QLatin1String("orthogonal") ? ConnectorStyle::Orthogonal
                                            : ConnectorStyle::Rounded;
}

QJsonObject NodeStyle::toJson() const {
    QJsonObject o;
    o[QStringLiteral("shape")] = mindflow::toString(shape);
    o[QStringLiteral("fill")] = fillColor.name(QColor::HexArgb);
    o[QStringLiteral("text")] = textColor.name(QColor::HexArgb);
    o[QStringLiteral("border")] = borderColor.name(QColor::HexArgb);
    o[QStringLiteral("font")] = font.toString();
    o[QStringLiteral("connector")] = mindflow::toString(connector);
    return o;
}

NodeStyle NodeStyle::fromJson(const QJsonObject& o) {
    NodeStyle s;
    if (o.isEmpty())
        return s;
    s.shape = nodeShapeFromString(o.value(QStringLiteral("shape")).toString());
    s.fillColor = QColor(o.value(QStringLiteral("fill")).toString());
    s.textColor = QColor(o.value(QStringLiteral("text")).toString());
    s.borderColor = QColor(o.value(QStringLiteral("border")).toString());
    if (o.contains(QStringLiteral("font")))
        s.font.fromString(o.value(QStringLiteral("font")).toString());
    s.connector = connectorStyleFromString(o.value(QStringLiteral("connector")).toString());
    return s;
}

} // namespace mindflow
