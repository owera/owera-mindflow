#pragma once

#include "model/Connection.h"
#include "model/Node.h"
#include "model/NodeStyle.h"
#include "model/Theme.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QUndoCommand>

#include <memory>

namespace mindflow {

class Document;

// Each command owns enough state to undo itself. Nodes removed from the tree are
// kept alive inside the command (via unique_ptr) until the command is destroyed,
// so redo/undo of deletions does not lose the subtree.

class AddNodeCommand : public QUndoCommand {
public:
    AddNodeCommand(Document* doc, Node* parent, const QString& text, int index = -1);
    void undo() override;
    void redo() override;
    Node* addedNode() const { return m_added; }

private:
    Document* m_doc;
    int m_parentId;
    int m_index;
    QString m_text;
    Node* m_added = nullptr;            // borrowed while in the tree
    std::unique_ptr<Node> m_storage;    // owns the node while undone
};

class RemoveNodeCommand : public QUndoCommand {
public:
    RemoveNodeCommand(Document* doc, Node* node);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_parentId;
    int m_index;
    int m_nodeId;
    std::unique_ptr<Node> m_storage;    // owns the node while removed
};

class SetTextCommand : public QUndoCommand {
public:
    SetTextCommand(Document* doc, Node* node, const QString& text);
    void undo() override;
    void redo() override;
    int id() const override { return 1001; }              // enable text-edit merging
    bool mergeWith(const QUndoCommand* other) override;

private:
    Document* m_doc;
    int m_nodeId;
    QString m_oldText;
    QString m_newText;
};

class MoveNodeCommand : public QUndoCommand {
public:
    MoveNodeCommand(Document* doc, Node* node, Node* newParent, int index,
                    const QPointF& scenePos);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_nodeId;
    int m_oldParentId;
    int m_oldIndex;
    QPointF m_oldPos;
    bool m_oldManual;
    int m_newParentId;
    int m_newIndex;
    QPointF m_newPos;
};

class SetPositionCommand : public QUndoCommand {
public:
    SetPositionCommand(Document* doc, Node* node, const QPointF& scenePos);
    void undo() override;
    void redo() override;
    int id() const override { return 1002; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    Document* m_doc;
    int m_nodeId;
    QPointF m_oldPos;
    bool m_oldManual;
    QPointF m_newPos;
};

class SetCollapsedCommand : public QUndoCommand {
public:
    SetCollapsedCommand(Document* doc, Node* node, bool collapsed);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_nodeId;
    bool m_oldValue;
    bool m_newValue;
};

class SetLayoutDirectionCommand : public QUndoCommand {
public:
    SetLayoutDirectionCommand(Document* doc, Node* node, LayoutDirection dir);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_nodeId;
    LayoutDirection m_oldValue;
    LayoutDirection m_newValue;
};

// Move a node + subtree out of its parent into a new top-level root.
class DetachRootCommand : public QUndoCommand {
public:
    DetachRootCommand(Document* doc, Node* node, const QPointF& scenePos);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_nodeId;
    int m_oldParentId;
    int m_oldIndex;
    QPointF m_oldPos;
    bool m_oldManual;
    QPointF m_newPos;
};

// Change a single node's visual style (shape, colors, font, connector).
class SetStyleCommand : public QUndoCommand {
public:
    SetStyleCommand(Document* doc, Node* node, const NodeStyle& style,
                    bool applyToSubtree = false);
    void undo() override;
    void redo() override;

private:
    void apply(const QHash<int, NodeStyle>& styles);

    Document* m_doc;
    int m_nodeId;
    bool m_subtree;
    NodeStyle m_newStyle;
    QHash<int, NodeStyle> m_oldStyles; // id -> prior style (subtree when applicable)
};

// Change a node's rich content (note, tags, sticker, task, image) as one step.
class SetContentCommand : public QUndoCommand {
public:
    SetContentCommand(Document* doc, Node* node, const NodeContent& content);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_nodeId;
    NodeContent m_old;
    NodeContent m_new;
};

// Switch the document theme and recolor every node by branch.
class ApplyThemeCommand : public QUndoCommand {
public:
    ApplyThemeCommand(Document* doc, const Theme& theme);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    Theme m_oldTheme;
    Theme m_newTheme;
    QHash<int, NodeStyle> m_oldStyles;
    bool m_captured = false;
};

// --- cross-connection commands ---------------------------------------------
class AddConnectionCommand : public QUndoCommand {
public:
    AddConnectionCommand(Document* doc, int fromId, int toId);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    Connection m_conn;
};

class RemoveConnectionCommand : public QUndoCommand {
public:
    RemoveConnectionCommand(Document* doc, int id);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_id;
    int m_index = -1;
    Connection m_stored;
};

class SetConnectionLabelCommand : public QUndoCommand {
public:
    SetConnectionLabelCommand(Document* doc, int id, const QString& label);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_id;
    QString m_old;
    QString m_new;
};

class SetConnectionWaypointCommand : public QUndoCommand {
public:
    SetConnectionWaypointCommand(Document* doc, int id, const QPointF& offset);
    void undo() override;
    void redo() override;
    int id() const override { return 1003; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    Document* m_doc;
    int m_id;
    QPointF m_old;
    QPointF m_new;
};

// Remove an entire top-level root (kept alive for undo).
class RemoveRootCommand : public QUndoCommand {
public:
    RemoveRootCommand(Document* doc, Node* root);
    void undo() override;
    void redo() override;

private:
    Document* m_doc;
    int m_rootId;
    int m_index;
    std::unique_ptr<Node> m_storage;
};

} // namespace mindflow
