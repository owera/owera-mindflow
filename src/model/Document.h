#pragma once

#include "model/Connection.h"
#include "model/Node.h"
#include "model/Theme.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <vector>

class QUndoStack;

namespace mindflow {

// Owns the mind-map tree(s) and the undo stack, and is the single source of change
// notifications. The UI never mutates Nodes directly: it calls the high-level ops
// here, which wrap each change in a QUndoCommand and emit the matching signal.
//
// A document holds one or more independent root trees, so several "main nodes" /
// detached trees can coexist on one canvas. root() returns the primary root.
class Document : public QObject {
    Q_OBJECT
public:
    explicit Document(QObject* parent = nullptr);
    ~Document() override;

    Node* root() const { return m_roots.empty() ? nullptr : m_roots.front().get(); }
    const std::vector<std::unique_ptr<Node>>& roots() const { return m_roots; }
    QUndoStack* undoStack() const { return m_undo; }

    // Depth-first lookup by stable id across all roots; nullptr if absent. Used by
    // commands to re-resolve nodes across undo/redo (ids are stable, pointers move).
    Node* nodeById(int id) const;
    // True if `node` is a top-level root (has no parent and is in m_roots).
    bool isRoot(const Node* node) const;
    // Sorted, de-duplicated list of every tag used in the document.
    QStringList allTags() const;

    QString title() const { return m_title; }
    void setTitle(const QString& title) { m_title = title; }

    const Theme& theme() const { return m_theme; }
    // Switch theme and recolor every node by branch (undoable). Emits themeChanged.
    void applyTheme(const Theme& theme);
    // Assign a new child's branch color/text from the theme and its parent.
    void styleNewChild(Node* child, Node* parent) const;

    // --- high-level, undoable operations -------------------------------------
    // Create a child of `parent` and return the new node (already in the tree).
    Node* addChild(Node* parent, const QString& text = QString());
    // Add a sibling after `node`.
    Node* addSibling(Node* node, const QString& text = QString());
    void setNodeText(Node* node, const QString& text);
    void removeNode(Node* node);                  // no-op on the last root
    // Reparent `node` under `newParent` at `index`, recording a manual position.
    void moveNode(Node* node, Node* newParent, int index, const QPointF& scenePos);
    void setNodePosition(Node* node, const QPointF& scenePos);
    void setCollapsed(Node* node, bool collapsed);
    void setLayoutDirection(Node* node, LayoutDirection dir);
    // Set a node's full style; if applyToSubtree, propagate shape/connector/font
    // to descendants (keeping their own branch colors).
    void setNodeStyle(Node* node, const NodeStyle& style, bool applyToSubtree = false);
    void setNodeContent(Node* node, const NodeContent& content);
    void toggleTaskDone(Node* node); // toggles done (making it a task if needed)

    // --- cross-connections ---------------------------------------------------
    const std::vector<Connection>& connections() const { return m_connections; }
    Connection* connectionById(int id);
    void addConnection(Node* from, Node* to);
    void removeConnection(int id);
    void setConnectionLabel(int id, const QString& label);
    void setConnectionWaypoint(int id, const QPointF& offset);
    // Detach `node` (with its subtree) from its parent into a new top-level root
    // anchored at scenePos.
    void detachAsRoot(Node* node, const QPointF& scenePos);

    // --- serialization (delegates to DocumentStore for file I/O) -------------
    QJsonObject toJson() const;
    void loadFromJson(const QJsonObject&);
    void reset(const QString& rootText = QStringLiteral("Central Idea"));
    // Replace all content with imported roots (recolors them by the current theme).
    void setImportedRoots(std::vector<std::unique_ptr<Node>> roots,
                          const QString& title);

    // Internal hooks used by command objects; not for UI use.
    void emitStructureChanged() { emit structureChanged(); }
    void emitNodeChanged(Node* n) { emit nodeChanged(n); }
    void emitThemeChanged() { emit themeChanged(); }
    void insertConnectionInternal(const Connection& c, int index = -1);
    Connection takeConnectionInternal(int id); // returns removed (id<=0 if absent)
    int connectionIndex(int id) const;
    void mutateConnection(int id, const QString* label, const QPointF* offset);
    void setThemeInternal(const Theme& theme);
    void recolorAllByBranch();
    void forEachNode(const std::function<void(Node*)>& fn) const;
    // Insert/extract a top-level root (used by detach/undo). index -1 appends.
    void insertRootInternal(std::unique_ptr<Node> root, int index = -1);
    std::unique_ptr<Node> takeRootInternal(Node* root);
    int rootIndex(const Node* root) const;

signals:
    void structureChanged();      // tree shape changed -> relayout + rebuild items
    void nodeChanged(Node* node); // a single node's content changed
    void themeChanged();          // theme switched -> update canvas + restyle items
    void modified();              // any change occurred (for window-modified state)

private:
    std::vector<std::unique_ptr<Node>> m_roots;
    std::vector<Connection> m_connections;
    QUndoStack* m_undo;
    QString m_title;
    Theme m_theme;
};

} // namespace mindflow
