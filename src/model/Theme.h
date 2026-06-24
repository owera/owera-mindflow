#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace mindflow {

// A named visual theme: the canvas color, the root node fill, default text color,
// and a palette of branch colors cycled across top-level branches for a colorful
// look. Stored in the document so a map reopens with its theme intact.
struct Theme {
    QString name = QStringLiteral("Default");
    bool dark = false;
    QColor canvas = QColor(0xf5, 0xf6, 0xf8);
    QColor rootFill = QColor(0x33, 0x3a, 0x46);
    QColor textOnFill = QColor(Qt::white);
    QColor textOnLight = QColor(0x22, 0x26, 0x2c);
    QVector<QColor> branchPalette;

    // Branch color for the Nth top-level branch (cycles through the palette).
    QColor branchColor(int index) const;

    QJsonObject toJson() const;
    static Theme fromJson(const QJsonObject&);

    // Built-in themes (first is the default).
    static QVector<Theme> builtIns();
    static Theme byName(const QString& name);
};

} // namespace mindflow
