#pragma once

#include <QString>

class QGraphicsScene;
class QColor;

namespace mindflow {

class Document;

// Export the document to various formats. Text formats derive from the model tree;
// image/vector/PDF formats render the canvas scene.
namespace exporters {

// --- model-based (return the serialized text) ---
QString toMarkdown(const Document& doc);
QString toOpml(const Document& doc);
QString toFreeMind(const Document& doc);
QString toCsv(const Document& doc);
QString toPlainText(const Document& doc);

// Write `text` to `path` (UTF-8). Returns false on failure.
bool writeTextFile(const QString& path, const QString& text);

// --- scene-based (return success) ---
bool toPng(QGraphicsScene* scene, const QString& path, const QColor& background);
bool toSvg(QGraphicsScene* scene, const QString& path);
bool toPdf(QGraphicsScene* scene, const QString& path);

} // namespace exporters
} // namespace mindflow
