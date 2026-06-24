#include "model/Commands.h"

#include "model/Document.h"

#include <functional>

namespace mindflow {

// ---------------------------------------------------------------- AddNode -----
AddNodeCommand::AddNodeCommand(Document* doc, Node* parent, const QString& text, int index)
    : m_doc(doc), m_parentId(parent->id()), m_index(index), m_text(text) {
    setText(QStringLiteral("Add Node"));
}

void AddNodeCommand::redo() {
    Node* parent = m_doc->nodeById(m_parentId);
    if (!parent)
        return;
    std::unique_ptr<Node> node;
    if (m_storage) {
        node = std::move(m_storage); // re-adding after undo: keep prior styling
    } else {
        node = std::make_unique<Node>(m_text);
        m_doc->styleNewChild(node.get(), parent); // inherit/assign branch color
    }
    m_added = parent->insertChild(std::move(node), m_index);
    m_doc->emitStructureChanged();
}

void AddNodeCommand::undo() {
    Node* parent = m_doc->nodeById(m_parentId);
    if (!parent || !m_added)
        return;
    m_storage = parent->takeChild(m_added);
    m_added = nullptr;
    m_doc->emitStructureChanged();
}

// ------------------------------------------------------------- RemoveNode -----
RemoveNodeCommand::RemoveNodeCommand(Document* doc, Node* node)
    : m_doc(doc), m_nodeId(node->id()) {
    m_parentId = node->parent() ? node->parent()->id() : -1;
    m_index = node->indexInParent();
    setText(QStringLiteral("Delete Node"));
}

void RemoveNodeCommand::redo() {
    Node* node = m_doc->nodeById(m_nodeId);
    if (!node || !node->parent())
        return;
    m_storage = node->parent()->takeChild(node);
    m_doc->emitStructureChanged();
}

void RemoveNodeCommand::undo() {
    Node* parent = m_doc->nodeById(m_parentId);
    if (!parent || !m_storage)
        return;
    parent->insertChild(std::move(m_storage), m_index);
    m_doc->emitStructureChanged();
}

// ---------------------------------------------------------------- SetText -----
SetTextCommand::SetTextCommand(Document* doc, Node* node, const QString& text)
    : m_doc(doc), m_nodeId(node->id()), m_oldText(node->text()), m_newText(text) {
    setText(QStringLiteral("Edit Text"));
}

void SetTextCommand::redo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->setText(m_newText);
        m_doc->emitNodeChanged(n);
    }
}

void SetTextCommand::undo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->setText(m_oldText);
        m_doc->emitNodeChanged(n);
    }
}

bool SetTextCommand::mergeWith(const QUndoCommand* other) {
    const auto* o = static_cast<const SetTextCommand*>(other);
    if (o->m_nodeId != m_nodeId)
        return false;
    m_newText = o->m_newText; // collapse consecutive keystrokes into one undo step
    return true;
}

// --------------------------------------------------------------- MoveNode -----
MoveNodeCommand::MoveNodeCommand(Document* doc, Node* node, Node* newParent, int index,
                                 const QPointF& scenePos)
    : m_doc(doc), m_nodeId(node->id()),
      m_oldParentId(node->parent() ? node->parent()->id() : -1),
      m_oldIndex(node->indexInParent()), m_oldPos(node->layoutPos),
      m_oldManual(node->hasManualPos), m_newParentId(newParent->id()),
      m_newIndex(index), m_newPos(scenePos) {
    setText(QStringLiteral("Move Node"));
}

void MoveNodeCommand::redo() {
    Node* node = m_doc->nodeById(m_nodeId);
    Node* newParent = m_doc->nodeById(m_newParentId);
    if (!node || !newParent || !node->parent())
        return;
    std::unique_ptr<Node> owned = node->parent()->takeChild(node);
    Node* moved = newParent->insertChild(std::move(owned), m_newIndex);
    moved->layoutPos = m_newPos;
    moved->hasManualPos = true;
    m_doc->emitStructureChanged();
}

void MoveNodeCommand::undo() {
    Node* node = m_doc->nodeById(m_nodeId);
    Node* oldParent = m_doc->nodeById(m_oldParentId);
    if (!node || !oldParent || !node->parent())
        return;
    std::unique_ptr<Node> owned = node->parent()->takeChild(node);
    Node* moved = oldParent->insertChild(std::move(owned), m_oldIndex);
    moved->layoutPos = m_oldPos;
    moved->hasManualPos = m_oldManual;
    m_doc->emitStructureChanged();
}

// ------------------------------------------------------------ SetPosition -----
SetPositionCommand::SetPositionCommand(Document* doc, Node* node, const QPointF& scenePos)
    : m_doc(doc), m_nodeId(node->id()), m_oldPos(node->layoutPos),
      m_oldManual(node->hasManualPos), m_newPos(scenePos) {
    setText(QStringLiteral("Move Node"));
}

void SetPositionCommand::redo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->layoutPos = m_newPos;
        n->hasManualPos = true;
        m_doc->emitStructureChanged();
    }
}

void SetPositionCommand::undo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->layoutPos = m_oldPos;
        n->hasManualPos = m_oldManual;
        m_doc->emitStructureChanged();
    }
}

bool SetPositionCommand::mergeWith(const QUndoCommand* other) {
    const auto* o = static_cast<const SetPositionCommand*>(other);
    if (o->m_nodeId != m_nodeId)
        return false;
    m_newPos = o->m_newPos;
    return true;
}

// ----------------------------------------------------------- SetCollapsed -----
SetCollapsedCommand::SetCollapsedCommand(Document* doc, Node* node, bool collapsed)
    : m_doc(doc), m_nodeId(node->id()), m_oldValue(node->collapsed),
      m_newValue(collapsed) {
    setText(collapsed ? QStringLiteral("Collapse") : QStringLiteral("Expand"));
}

void SetCollapsedCommand::redo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->collapsed = m_newValue;
        m_doc->emitStructureChanged();
    }
}

void SetCollapsedCommand::undo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->collapsed = m_oldValue;
        m_doc->emitStructureChanged();
    }
}

// ------------------------------------------------------ SetLayoutDirection -----
SetLayoutDirectionCommand::SetLayoutDirectionCommand(Document* doc, Node* node,
                                                     LayoutDirection dir)
    : m_doc(doc), m_nodeId(node->id()), m_oldValue(node->layoutDirection),
      m_newValue(dir) {
    setText(QStringLiteral("Change Layout"));
}

void SetLayoutDirectionCommand::redo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->layoutDirection = m_newValue;
        m_doc->emitStructureChanged();
    }
}

void SetLayoutDirectionCommand::undo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->layoutDirection = m_oldValue;
        m_doc->emitStructureChanged();
    }
}

// ------------------------------------------------------------- DetachRoot -----
DetachRootCommand::DetachRootCommand(Document* doc, Node* node, const QPointF& scenePos)
    : m_doc(doc), m_nodeId(node->id()),
      m_oldParentId(node->parent() ? node->parent()->id() : -1),
      m_oldIndex(node->indexInParent()), m_oldPos(node->layoutPos),
      m_oldManual(node->hasManualPos), m_newPos(scenePos) {
    setText(QStringLiteral("Detach Node"));
}

void DetachRootCommand::redo() {
    Node* node = m_doc->nodeById(m_nodeId);
    if (!node || !node->parent())
        return;
    std::unique_ptr<Node> owned = node->parent()->takeChild(node);
    owned->layoutPos = m_newPos;
    owned->hasManualPos = true;
    m_doc->insertRootInternal(std::move(owned));
    m_doc->emitStructureChanged();
}

void DetachRootCommand::undo() {
    Node* node = m_doc->nodeById(m_nodeId);
    Node* oldParent = m_doc->nodeById(m_oldParentId);
    if (!node || !oldParent)
        return;
    std::unique_ptr<Node> owned = m_doc->takeRootInternal(node);
    if (!owned)
        return;
    owned->layoutPos = m_oldPos;
    owned->hasManualPos = m_oldManual;
    oldParent->insertChild(std::move(owned), m_oldIndex);
    m_doc->emitStructureChanged();
}

// --------------------------------------------------------------- SetStyle -----
SetStyleCommand::SetStyleCommand(Document* doc, Node* node, const NodeStyle& style,
                                 bool applyToSubtree)
    : m_doc(doc), m_nodeId(node->id()), m_subtree(applyToSubtree), m_newStyle(style) {
    setText(QStringLiteral("Change Style"));
    std::function<void(Node*)> capture = [&](Node* n) {
        m_oldStyles.insert(n->id(), n->style);
        if (m_subtree)
            for (const auto& c : n->children())
                capture(c.get());
    };
    capture(node);
}

void SetStyleCommand::apply(const QHash<int, NodeStyle>& styles) {
    for (auto it = styles.constBegin(); it != styles.constEnd(); ++it)
        if (Node* n = m_doc->nodeById(it.key()))
            n->style = it.value();
    m_doc->emitStructureChanged();
}

void SetStyleCommand::redo() {
    Node* node = m_doc->nodeById(m_nodeId);
    if (!node)
        return;
    // Preserve each node's own fill when applying to a subtree only changes the
    // non-color attributes; here we set the full style on the target, and for the
    // subtree we propagate shape/connector/font but keep per-node colors.
    std::function<void(Node*, bool)> set = [&](Node* n, bool isTarget) {
        if (isTarget) {
            n->style = m_newStyle;
        } else {
            n->style.shape = m_newStyle.shape;
            n->style.connector = m_newStyle.connector;
            n->style.font = m_newStyle.font;
        }
        if (m_subtree)
            for (const auto& c : n->children())
                set(c.get(), false);
    };
    set(node, true);
    m_doc->emitStructureChanged();
}

void SetStyleCommand::undo() { apply(m_oldStyles); }

// ------------------------------------------------------------- SetContent -----
SetContentCommand::SetContentCommand(Document* doc, Node* node, const NodeContent& content)
    : m_doc(doc), m_nodeId(node->id()), m_old(node->content()), m_new(content) {
    setText(QStringLiteral("Edit Content"));
}

void SetContentCommand::redo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->setContent(m_new);
        m_doc->emitStructureChanged(); // size/visuals may change
    }
}

void SetContentCommand::undo() {
    if (Node* n = m_doc->nodeById(m_nodeId)) {
        n->setContent(m_old);
        m_doc->emitStructureChanged();
    }
}

// ------------------------------------------------------------- ApplyTheme -----
ApplyThemeCommand::ApplyThemeCommand(Document* doc, const Theme& theme)
    : m_doc(doc), m_oldTheme(doc->theme()), m_newTheme(theme) {
    setText(QStringLiteral("Change Theme"));
}

void ApplyThemeCommand::redo() {
    if (!m_captured) {
        m_doc->forEachNode([this](Node* n) { m_oldStyles.insert(n->id(), n->style); });
        m_captured = true;
    }
    m_doc->setThemeInternal(m_newTheme);
    m_doc->recolorAllByBranch();
    m_doc->emitThemeChanged();
    m_doc->emitStructureChanged();
}

void ApplyThemeCommand::undo() {
    m_doc->setThemeInternal(m_oldTheme);
    for (auto it = m_oldStyles.constBegin(); it != m_oldStyles.constEnd(); ++it)
        if (Node* n = m_doc->nodeById(it.key()))
            n->style = it.value();
    m_doc->emitThemeChanged();
    m_doc->emitStructureChanged();
}

// ----------------------------------------------------------- Connections -----
AddConnectionCommand::AddConnectionCommand(Document* doc, int fromId, int toId)
    : m_doc(doc), m_conn(fromId, toId) {
    setText(QStringLiteral("Add Connection"));
}

void AddConnectionCommand::redo() {
    m_doc->insertConnectionInternal(m_conn);
    m_doc->emitStructureChanged();
}

void AddConnectionCommand::undo() {
    m_doc->takeConnectionInternal(m_conn.id);
    m_doc->emitStructureChanged();
}

RemoveConnectionCommand::RemoveConnectionCommand(Document* doc, int id)
    : m_doc(doc), m_id(id) {
    setText(QStringLiteral("Delete Connection"));
}

void RemoveConnectionCommand::redo() {
    m_index = m_doc->connectionIndex(m_id);
    m_stored = m_doc->takeConnectionInternal(m_id);
    m_doc->emitStructureChanged();
}

void RemoveConnectionCommand::undo() {
    m_doc->insertConnectionInternal(m_stored, m_index);
    m_doc->emitStructureChanged();
}

SetConnectionLabelCommand::SetConnectionLabelCommand(Document* doc, int id,
                                                     const QString& label)
    : m_doc(doc), m_id(id), m_new(label) {
    if (Connection* c = doc->connectionById(id))
        m_old = c->label;
    setText(QStringLiteral("Label Connection"));
}

void SetConnectionLabelCommand::redo() {
    m_doc->mutateConnection(m_id, &m_new, nullptr);
    m_doc->emitStructureChanged();
}

void SetConnectionLabelCommand::undo() {
    m_doc->mutateConnection(m_id, &m_old, nullptr);
    m_doc->emitStructureChanged();
}

SetConnectionWaypointCommand::SetConnectionWaypointCommand(Document* doc, int id,
                                                           const QPointF& offset)
    : m_doc(doc), m_id(id), m_new(offset) {
    if (Connection* c = doc->connectionById(id))
        m_old = c->waypointOffset;
    setText(QStringLiteral("Curve Connection"));
}

void SetConnectionWaypointCommand::redo() {
    m_doc->mutateConnection(m_id, nullptr, &m_new);
    m_doc->emitStructureChanged();
}

void SetConnectionWaypointCommand::undo() {
    m_doc->mutateConnection(m_id, nullptr, &m_old);
    m_doc->emitStructureChanged();
}

bool SetConnectionWaypointCommand::mergeWith(const QUndoCommand* other) {
    const auto* o = static_cast<const SetConnectionWaypointCommand*>(other);
    if (o->m_id != m_id)
        return false;
    m_new = o->m_new;
    return true;
}

// ------------------------------------------------------------- RemoveRoot -----
RemoveRootCommand::RemoveRootCommand(Document* doc, Node* root)
    : m_doc(doc), m_rootId(root->id()), m_index(doc->rootIndex(root)) {
    setText(QStringLiteral("Delete Tree"));
}

void RemoveRootCommand::redo() {
    if (Node* root = m_doc->nodeById(m_rootId)) {
        m_storage = m_doc->takeRootInternal(root);
        m_doc->emitStructureChanged();
    }
}

void RemoveRootCommand::undo() {
    if (m_storage) {
        m_doc->insertRootInternal(std::move(m_storage), m_index);
        m_doc->emitStructureChanged();
    }
}

} // namespace mindflow
