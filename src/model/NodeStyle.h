#pragma once

#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QString>

namespace mindflow {

// The node shapes Owera MindFlow can draw. Only a subset is fully rendered in M1;
// the enum is complete so the data model and file format are stable from the start.
enum class NodeShape {
    Line,
    Embedded,
    Rectangle,
    Rounded,
    Pill,
    Cloud,
    Hexagon,
    Octagon,
};

// How branches grow from a node. Per-node so layouts can be mixed across the map.
enum class LayoutDirection {
    Organic,     // radial / free organic spread (default)
    RightDown,   // horizontal, children to the right
    LeftDown,    // horizontal, children to the left
    Down,        // vertical, top -> bottom
    Up,          // vertical, bottom -> top
    Compact,
};

enum class ConnectorStyle {
    Rounded,     // cubic bezier (the signature organic look)
    Orthogonal,  // elbow segments
};

QString toString(NodeShape);
NodeShape nodeShapeFromString(const QString&);
QString toString(LayoutDirection);
LayoutDirection layoutDirectionFromString(const QString&);
QString toString(ConnectorStyle);
ConnectorStyle connectorStyleFromString(const QString&);

// Visual style for a single node. A node inherits unset values (notably branch
// color) from its parent; M1 keeps a flat copy per node for simplicity.
struct NodeStyle {
    NodeShape shape = NodeShape::Rounded;
    QColor fillColor = QColor(0x2d, 0x6c, 0xdf);   // default branch blue
    QColor textColor = QColor(Qt::white);
    QColor borderColor = QColor(Qt::transparent);
    QFont font = QFont(QStringLiteral("Sans"), 12);
    ConnectorStyle connector = ConnectorStyle::Rounded;

    QJsonObject toJson() const;
    static NodeStyle fromJson(const QJsonObject&);
};

} // namespace mindflow
