#pragma once

#include "model/NodeStyle.h"

#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace mindflow {

// The editable "rich content" of a node, snapshotted as a unit so a single
// undo command can capture any combination of changes (note/tags/sticker/task/image).
struct NodeContent {
    QString note;
    QStringList tags;
    QString sticker;          // emoji / short glyph; empty = none
    bool isTask = false;
    bool taskDone = false;
    QByteArray imagePng;      // raw PNG bytes; empty = no image
};

// A single idea in the mind map. Owns its children. Not a QObject: the model is a
// plain data tree, and change notification is centralised in Document so that both
// the canvas and (later) the outline view observe one signal source.
class Node {
public:
    explicit Node(QString text = QString());

    int id() const { return m_id; }

    const QString& text() const { return m_text; }
    void setText(const QString& text) { m_text = text; }

    Node* parent() const { return m_parent; }
    const std::vector<std::unique_ptr<Node>>& children() const { return m_children; }
    bool isLeaf() const { return m_children.empty(); }
    int indexInParent() const;

    // --- structural mutation (called by Document / commands, not the UI directly) ---
    // Insert an owned child at index (-1 = append) and return a borrowed pointer.
    Node* insertChild(std::unique_ptr<Node> child, int index = -1);
    // Detach a direct child, transferring ownership back to the caller.
    std::unique_ptr<Node> takeChild(Node* child);

    // --- presentation / content ---
    NodeStyle style;
    LayoutDirection layoutDirection = LayoutDirection::Organic; // how children grow
    QPointF layoutPos;            // scene position assigned by the LayoutEngine
    bool hasManualPos = false;    // user dragged this node; layout should respect it
    bool collapsed = false;       // subtree folded away
    QString note;                 // hidden long-form detail
    QStringList tags;             // visual tags
    QString sticker;              // emoji / glyph shown before the text
    bool isTask = false;          // render a checkbox
    bool taskDone = false;        // checkbox state
    QByteArray imagePng;          // attached image (PNG bytes); empty = none

    // Bundle access for undoable content edits.
    NodeContent content() const;
    void setContent(const NodeContent& c);

    // Task progress over this subtree: {done, total} counting task nodes.
    void taskProgress(int& done, int& total) const;

    // --- serialization ---
    QJsonObject toJson() const;
    static std::unique_ptr<Node> fromJson(const QJsonObject&);

private:
    void setParent(Node* parent) { m_parent = parent; }
    void setId(int id);

    int m_id;
    QString m_text;
    Node* m_parent = nullptr;
    std::vector<std::unique_ptr<Node>> m_children;
};

} // namespace mindflow
