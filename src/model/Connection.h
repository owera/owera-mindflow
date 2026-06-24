#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>

namespace mindflow {

// A free "cross-connection" between any two nodes, independent of the parent-child
// hierarchy. Endpoints are stored by stable node id. waypointOffset displaces the
// curve's control point from the midpoint of the two endpoints; (0,0) draws a
// straight line. Each connection has its own stable id.
struct Connection {
    int id = 0;
    int fromId = -1;
    int toId = -1;
    QString label;
    QPointF waypointOffset; // (0,0) => straight

    Connection() = default;
    Connection(int from, int to);

    QJsonObject toJson() const;
    static Connection fromJson(const QJsonObject&);
};

} // namespace mindflow
