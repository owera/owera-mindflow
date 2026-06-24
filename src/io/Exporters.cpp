#include "io/Exporters.h"

#include "model/Document.h"
#include "model/Node.h"

#include <QFile>
#include <QGraphicsScene>
#include <QPainter>
#include <QPdfWriter>
#include <QTextStream>
#include <QtSvg/QSvgGenerator>

namespace mindflow {
namespace exporters {

namespace {

QString xmlEscape(const QString& s) {
    QString out = s;
    out.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    out.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    out.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    out.replace(QLatin1Char('"'), QLatin1String("&quot;"));
    return out;
}

QString csvEscape(const QString& s) {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) ||
        s.contains(QLatin1Char('\n'))) {
        QString q = s;
        q.replace(QLatin1Char('"'), QLatin1String("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    }
    return s;
}

void markdownNode(const Node* n, int depth, QString& out) {
    if (depth == 0) {
        out += QStringLiteral("# %1\n\n").arg(n->text());
    } else {
        out += QString(2 * (depth - 1), QLatin1Char(' '));
        out += QStringLiteral("- ");
        if (n->isTask)
            out += n->taskDone ? QStringLiteral("[x] ") : QStringLiteral("[ ] ");
        out += n->text();
        for (const QString& tag : n->tags)
            out += QStringLiteral(" #%1").arg(tag);
        out += QLatin1Char('\n');
        if (!n->note.isEmpty())
            out += QString(2 * depth, QLatin1Char(' ')) +
                   QStringLiteral("> %1\n").arg(n->note);
    }
    for (const auto& c : n->children())
        markdownNode(c.get(), depth + 1, out);
}

void opmlNode(const Node* n, int indent, QString& out) {
    const QString pad(indent * 2, QLatin1Char(' '));
    QString attrs = QStringLiteral("text=\"%1\"").arg(xmlEscape(n->text()));
    if (!n->note.isEmpty())
        attrs += QStringLiteral(" _note=\"%1\"").arg(xmlEscape(n->note));
    if (n->children().empty()) {
        out += pad + QStringLiteral("<outline %1/>\n").arg(attrs);
    } else {
        out += pad + QStringLiteral("<outline %1>\n").arg(attrs);
        for (const auto& c : n->children())
            opmlNode(c.get(), indent + 1, out);
        out += pad + QStringLiteral("</outline>\n");
    }
}

void writeFreeMindNode(const Node* n, QString& out) {
    out += QStringLiteral("<node TEXT=\"%1\"").arg(xmlEscape(n->text()));
    if (n->children().empty()) {
        out += QStringLiteral("/>\n");
    } else {
        out += QStringLiteral(">\n");
        for (const auto& c : n->children())
            writeFreeMindNode(c.get(), out);
        out += QStringLiteral("</node>\n");
    }
}

void csvNode(const Node* n, int depth, QString& out) {
    out += QString(depth, QLatin1Char(','));
    out += csvEscape(n->text());
    out += QLatin1Char('\n');
    for (const auto& c : n->children())
        csvNode(c.get(), depth + 1, out);
}

void plainNode(const Node* n, int depth, QString& out) {
    out += QString(depth, QLatin1Char('\t')) + n->text() + QLatin1Char('\n');
    for (const auto& c : n->children())
        plainNode(c.get(), depth + 1, out);
}

bool renderToPaintDevice(QGraphicsScene* scene, QPainter* painter, const QRectF& source,
                         const QRectF& target) {
    scene->render(painter, target, source);
    return true;
}

} // namespace

QString toMarkdown(const Document& doc) {
    QString out;
    for (const auto& root : doc.roots())
        markdownNode(root.get(), 0, out);
    return out;
}

QString toOpml(const Document& doc) {
    QString out = QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                 "<opml version=\"2.0\">\n  <head><title>%1</title></head>\n"
                                 "  <body>\n")
                      .arg(xmlEscape(doc.title()));
    for (const auto& root : doc.roots())
        opmlNode(root.get(), 2, out);
    out += QStringLiteral("  </body>\n</opml>\n");
    return out;
}

QString toFreeMind(const Document& doc) {
    QString out = QStringLiteral("<map version=\"1.0.1\">\n");
    for (const auto& root : doc.roots())
        writeFreeMindNode(root.get(), out);
    out += QStringLiteral("</map>\n");
    return out;
}

QString toCsv(const Document& doc) {
    QString out;
    for (const auto& root : doc.roots())
        csvNode(root.get(), 0, out);
    return out;
}

QString toPlainText(const Document& doc) {
    QString out;
    for (const auto& root : doc.roots())
        plainNode(root.get(), 0, out);
    return out;
}

bool writeTextFile(const QString& path, const QString& text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream ts(&file);
    ts << text;
    return true;
}

bool toPng(QGraphicsScene* scene, const QString& path, const QColor& background) {
    if (!scene)
        return false;
    QRectF bounds = scene->itemsBoundingRect().adjusted(-30, -30, 30, 30);
    if (bounds.isEmpty())
        return false;
    QImage image(bounds.size().toSize(), QImage::Format_ARGB32);
    image.fill(background);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderToPaintDevice(scene, &painter, bounds, QRectF(QPointF(0, 0), bounds.size()));
    painter.end();
    return image.save(path, "PNG");
}

bool toSvg(QGraphicsScene* scene, const QString& path) {
    if (!scene)
        return false;
    QRectF bounds = scene->itemsBoundingRect().adjusted(-30, -30, 30, 30);
    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(bounds.size().toSize());
    gen.setViewBox(QRectF(QPointF(0, 0), bounds.size()));
    gen.setTitle(QStringLiteral("Owera MindFlow"));
    QPainter painter(&gen);
    renderToPaintDevice(scene, &painter, bounds, QRectF(QPointF(0, 0), bounds.size()));
    painter.end();
    return true;
}

bool toPdf(QGraphicsScene* scene, const QString& path) {
    if (!scene)
        return false;
    QRectF bounds = scene->itemsBoundingRect().adjusted(-30, -30, 30, 30);
    if (bounds.isEmpty())
        return false;
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(bounds.size().toSize()));
    writer.setResolution(96);
    QPainter painter(&writer);
    renderToPaintDevice(scene, &painter, bounds, painter.viewport());
    painter.end();
    return true;
}

} // namespace exporters
} // namespace mindflow
