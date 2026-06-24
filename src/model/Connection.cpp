#include "model/Connection.h"

#include <algorithm>

namespace mindflow {

namespace {
int g_nextConnId = 1;
} // namespace

Connection::Connection(int from, int to)
    : id(g_nextConnId++), fromId(from), toId(to) {}

QJsonObject Connection::toJson() const {
    QJsonObject o;
    o[QStringLiteral("id")] = id;
    o[QStringLiteral("from")] = fromId;
    o[QStringLiteral("to")] = toId;
    if (!label.isEmpty())
        o[QStringLiteral("label")] = label;
    o[QStringLiteral("wx")] = waypointOffset.x();
    o[QStringLiteral("wy")] = waypointOffset.y();
    return o;
}

Connection Connection::fromJson(const QJsonObject& o) {
    Connection c;
    c.id = o.value(QStringLiteral("id")).toInt();
    c.fromId = o.value(QStringLiteral("from")).toInt(-1);
    c.toId = o.value(QStringLiteral("to")).toInt(-1);
    c.label = o.value(QStringLiteral("label")).toString();
    c.waypointOffset = QPointF(o.value(QStringLiteral("wx")).toDouble(),
                               o.value(QStringLiteral("wy")).toDouble());
    g_nextConnId = std::max(g_nextConnId, c.id + 1);
    return c;
}

} // namespace mindflow
