#include "model/Theme.h"

#include <QJsonArray>

namespace mindflow {

QColor Theme::branchColor(int index) const {
    if (branchPalette.isEmpty())
        return QColor(0x2d, 0x6c, 0xdf);
    return branchPalette.at(((index % branchPalette.size()) + branchPalette.size()) %
                            branchPalette.size());
}

QJsonObject Theme::toJson() const {
    QJsonObject o;
    o[QStringLiteral("name")] = name;
    o[QStringLiteral("dark")] = dark;
    o[QStringLiteral("canvas")] = canvas.name(QColor::HexArgb);
    o[QStringLiteral("rootFill")] = rootFill.name(QColor::HexArgb);
    o[QStringLiteral("textOnFill")] = textOnFill.name(QColor::HexArgb);
    o[QStringLiteral("textOnLight")] = textOnLight.name(QColor::HexArgb);
    QJsonArray pal;
    for (const QColor& c : branchPalette)
        pal.append(c.name(QColor::HexArgb));
    o[QStringLiteral("palette")] = pal;
    return o;
}

Theme Theme::fromJson(const QJsonObject& o) {
    if (o.isEmpty())
        return builtIns().first();
    Theme t;
    t.name = o.value(QStringLiteral("name")).toString(QStringLiteral("Default"));
    t.dark = o.value(QStringLiteral("dark")).toBool();
    t.canvas = QColor(o.value(QStringLiteral("canvas")).toString());
    t.rootFill = QColor(o.value(QStringLiteral("rootFill")).toString());
    t.textOnFill = QColor(o.value(QStringLiteral("textOnFill")).toString());
    t.textOnLight = QColor(o.value(QStringLiteral("textOnLight")).toString());
    t.branchPalette.clear();
    for (const auto& c : o.value(QStringLiteral("palette")).toArray())
        t.branchPalette.append(QColor(c.toString()));
    if (t.branchPalette.isEmpty())
        t.branchPalette = builtIns().first().branchPalette;
    return t;
}

QVector<Theme> Theme::builtIns() {
    Theme light;
    light.name = QStringLiteral("Default");
    light.dark = false;
    light.canvas = QColor(0xf5, 0xf6, 0xf8);
    light.rootFill = QColor(0x33, 0x3a, 0x46);
    light.textOnFill = QColor(Qt::white);
    light.textOnLight = QColor(0x22, 0x26, 0x2c);
    light.branchPalette = {
        QColor(0x2d, 0x6c, 0xdf), QColor(0xe0, 0x6c, 0x2b), QColor(0x2f, 0xa8, 0x4f),
        QColor(0xc0, 0x3a, 0x6b), QColor(0x8b, 0x5c, 0xf6), QColor(0x0e, 0x9b, 0xa6),
        QColor(0xd1, 0x9a, 0x00),
    };

    Theme dark;
    dark.name = QStringLiteral("Midnight");
    dark.dark = true;
    dark.canvas = QColor(0x1b, 0x1e, 0x24);
    dark.rootFill = QColor(0xe6, 0xe9, 0xef);
    dark.textOnFill = QColor(0x11, 0x14, 0x19);
    dark.textOnLight = QColor(0xe6, 0xe9, 0xef);
    dark.branchPalette = {
        QColor(0x5b, 0x9c, 0xff), QColor(0xff, 0x9b, 0x52), QColor(0x57, 0xd1, 0x7a),
        QColor(0xff, 0x6f, 0xa3), QColor(0xb1, 0x8c, 0xff), QColor(0x42, 0xc9, 0xd6),
        QColor(0xf2, 0xc4, 0x4d),
    };

    Theme forest;
    forest.name = QStringLiteral("Forest");
    forest.dark = false;
    forest.canvas = QColor(0xf0, 0xf4, 0xee);
    forest.rootFill = QColor(0x2c, 0x3e, 0x35);
    forest.textOnFill = QColor(Qt::white);
    forest.textOnLight = QColor(0x22, 0x2c, 0x26);
    forest.branchPalette = {
        QColor(0x4c, 0x8c, 0x52), QColor(0x88, 0xa6, 0x4b), QColor(0x37, 0x8a, 0x7e),
        QColor(0xb5, 0x7e, 0x3c), QColor(0x6b, 0x9b, 0x37), QColor(0x2f, 0x6f, 0x5a),
    };

    return {light, dark, forest};
}

Theme Theme::byName(const QString& name) {
    for (const Theme& t : builtIns())
        if (t.name == name)
            return t;
    return builtIns().first();
}

} // namespace mindflow
