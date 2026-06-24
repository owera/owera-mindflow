#include "model/Document.h"

#include "model/Commands.h"

#include <QJsonArray>
#include <QUndoStack>

#include <functional>

namespace mindflow {

namespace {
Node* findInSubtree(Node* n, int id) {
    if (!n)
        return nullptr;
    if (n->id() == id)
        return n;
    for (const auto& c : n->children())
        if (Node* hit = findInSubtree(c.get(), id))
            return hit;
    return nullptr;
}

QColor textColorFor(const QColor& fill) {
    const double luminance =
        0.299 * fill.red() + 0.587 * fill.green() + 0.114 * fill.blue();
    return luminance < 150 ? QColor(Qt::white) : QColor(0x20, 0x24, 0x2a);
}

void recolorBranch(Node* node, const QColor& color) {
    node->style.fillColor = color;
    node->style.textColor = textColorFor(color);
    for (const auto& c : node->children())
        recolorBranch(c.get(), color);
}
} // namespace

Document::Document(QObject* parent)
    : QObject(parent), m_undo(new QUndoStack(this)) {
    m_theme = Theme::builtIns().first();
    reset();
    connect(m_undo, &QUndoStack::indexChanged, this, &Document::modified);
}

Document::~Document() = default;

Node* Document::nodeById(int id) const {
    for (const auto& root : m_roots)
        if (Node* hit = findInSubtree(root.get(), id))
            return hit;
    return nullptr;
}

bool Document::isRoot(const Node* node) const {
    if (!node || node->parent())
        return false;
    for (const auto& root : m_roots)
        if (root.get() == node)
            return true;
    return false;
}

QStringList Document::allTags() const {
    QStringList tags;
    forEachNode([&](Node* n) {
        for (const QString& t : n->tags)
            if (!tags.contains(t))
                tags << t;
    });
    tags.sort(Qt::CaseInsensitive);
    return tags;
}

int Document::rootIndex(const Node* root) const {
    for (int i = 0; i < static_cast<int>(m_roots.size()); ++i)
        if (m_roots[i].get() == root)
            return i;
    return -1;
}

void Document::reset(const QString& rootText) {
    m_undo->clear();
    m_connections.clear();
    m_roots.clear();
    auto root = std::make_unique<Node>(rootText);
    root->style.fillColor = m_theme.rootFill;
    root->style.textColor = textColorFor(m_theme.rootFill);
    m_roots.push_back(std::move(root));
    m_title = QStringLiteral("Untitled");
    emit structureChanged();
}

void Document::setImportedRoots(std::vector<std::unique_ptr<Node>> roots,
                                const QString& title) {
    m_undo->clear();
    m_connections.clear();
    m_roots = std::move(roots);
    if (m_roots.empty())
        m_roots.push_back(std::make_unique<Node>(QStringLiteral("Untitled")));
    m_title = title.isEmpty() ? QStringLiteral("Untitled") : title;
    recolorAllByBranch(); // give imported branches the theme's colors
    emit structureChanged();
}

void Document::styleNewChild(Node* child, Node* parent) const {
    if (!child || !parent)
        return;
    QColor fill;
    if (isRoot(parent)) {
        // New top-level branch: next palette color (index = its position).
        fill = m_theme.branchColor(static_cast<int>(parent->children().size()));
    } else {
        fill = parent->style.fillColor; // inherit the branch color
    }
    child->style.fillColor = fill;
    child->style.textColor = textColorFor(fill);
    child->style.connector = parent->style.connector;
    child->style.shape = parent->style.shape;
}

void Document::applyTheme(const Theme& theme) {
    m_undo->push(new ApplyThemeCommand(this, theme));
}

void Document::setThemeInternal(const Theme& theme) { m_theme = theme; }

void Document::recolorAllByBranch() {
    for (const auto& root : m_roots) {
        root->style.fillColor = m_theme.rootFill;
        root->style.textColor = textColorFor(m_theme.rootFill);
        int i = 0;
        for (const auto& child : root->children())
            recolorBranch(child.get(), m_theme.branchColor(i++));
    }
}

void Document::forEachNode(const std::function<void(Node*)>& fn) const {
    std::function<void(Node*)> walk = [&](Node* n) {
        fn(n);
        for (const auto& c : n->children())
            walk(c.get());
    };
    for (const auto& root : m_roots)
        walk(root.get());
}

void Document::insertRootInternal(std::unique_ptr<Node> root, int index) {
    if (index < 0 || index >= static_cast<int>(m_roots.size()))
        m_roots.push_back(std::move(root));
    else
        m_roots.insert(m_roots.begin() + index, std::move(root));
}

std::unique_ptr<Node> Document::takeRootInternal(Node* root) {
    for (auto it = m_roots.begin(); it != m_roots.end(); ++it) {
        if (it->get() == root) {
            std::unique_ptr<Node> owned = std::move(*it);
            m_roots.erase(it);
            return owned;
        }
    }
    return nullptr;
}

Node* Document::addChild(Node* parent, const QString& text) {
    if (!parent)
        return nullptr;
    auto* cmd = new AddNodeCommand(this, parent, text);
    m_undo->push(cmd);
    return cmd->addedNode();
}

Node* Document::addSibling(Node* node, const QString& text) {
    if (!node || !node->parent())
        return addChild(node, text); // a root has no siblings; fall back to a child
    auto* cmd = new AddNodeCommand(this, node->parent(), text, node->indexInParent() + 1);
    m_undo->push(cmd);
    return cmd->addedNode();
}

void Document::setNodeText(Node* node, const QString& text) {
    if (!node || node->text() == text)
        return;
    m_undo->push(new SetTextCommand(this, node, text));
}

void Document::removeNode(Node* node) {
    if (!node)
        return;
    if (isRoot(node)) {
        if (m_roots.size() <= 1)
            return; // keep at least one root
        m_undo->push(new RemoveRootCommand(this, node));
        return;
    }
    m_undo->push(new RemoveNodeCommand(this, node));
}

void Document::moveNode(Node* node, Node* newParent, int index, const QPointF& scenePos) {
    if (!node || !newParent || isRoot(node))
        return;
    m_undo->push(new MoveNodeCommand(this, node, newParent, index, scenePos));
}

void Document::setNodePosition(Node* node, const QPointF& scenePos) {
    if (!node)
        return;
    m_undo->push(new SetPositionCommand(this, node, scenePos));
}

void Document::setCollapsed(Node* node, bool collapsed) {
    if (!node || node->collapsed == collapsed)
        return;
    m_undo->push(new SetCollapsedCommand(this, node, collapsed));
}

void Document::setLayoutDirection(Node* node, LayoutDirection dir) {
    if (!node || node->layoutDirection == dir)
        return;
    m_undo->push(new SetLayoutDirectionCommand(this, node, dir));
}

void Document::setNodeStyle(Node* node, const NodeStyle& style, bool applyToSubtree) {
    if (!node)
        return;
    m_undo->push(new SetStyleCommand(this, node, style, applyToSubtree));
}

void Document::setNodeContent(Node* node, const NodeContent& content) {
    if (!node)
        return;
    m_undo->push(new SetContentCommand(this, node, content));
}

void Document::toggleTaskDone(Node* node) {
    if (!node)
        return;
    NodeContent c = node->content();
    c.isTask = true;
    c.taskDone = !c.taskDone;
    m_undo->push(new SetContentCommand(this, node, c));
}

// ---- connections -----------------------------------------------------------
Connection* Document::connectionById(int id) {
    for (auto& c : m_connections)
        if (c.id == id)
            return &c;
    return nullptr;
}

int Document::connectionIndex(int id) const {
    for (int i = 0; i < static_cast<int>(m_connections.size()); ++i)
        if (m_connections[i].id == id)
            return i;
    return -1;
}

void Document::insertConnectionInternal(const Connection& c, int index) {
    if (index < 0 || index >= static_cast<int>(m_connections.size()))
        m_connections.push_back(c);
    else
        m_connections.insert(m_connections.begin() + index, c);
}

Connection Document::takeConnectionInternal(int id) {
    const int idx = connectionIndex(id);
    if (idx < 0)
        return Connection();
    Connection c = m_connections[idx];
    m_connections.erase(m_connections.begin() + idx);
    return c;
}

void Document::mutateConnection(int id, const QString* label, const QPointF* offset) {
    if (Connection* c = connectionById(id)) {
        if (label)
            c->label = *label;
        if (offset)
            c->waypointOffset = *offset;
    }
}

void Document::addConnection(Node* from, Node* to) {
    if (!from || !to || from == to)
        return;
    // Avoid duplicate connections between the same unordered pair.
    for (const auto& c : m_connections) {
        if ((c.fromId == from->id() && c.toId == to->id()) ||
            (c.fromId == to->id() && c.toId == from->id()))
            return;
    }
    m_undo->push(new AddConnectionCommand(this, from->id(), to->id()));
}

void Document::removeConnection(int id) {
    if (connectionIndex(id) >= 0)
        m_undo->push(new RemoveConnectionCommand(this, id));
}

void Document::setConnectionLabel(int id, const QString& label) {
    Connection* c = connectionById(id);
    if (c && c->label != label)
        m_undo->push(new SetConnectionLabelCommand(this, id, label));
}

void Document::setConnectionWaypoint(int id, const QPointF& offset) {
    if (connectionById(id))
        m_undo->push(new SetConnectionWaypointCommand(this, id, offset));
}

void Document::detachAsRoot(Node* node, const QPointF& scenePos) {
    if (!node || isRoot(node) || !node->parent())
        return;
    m_undo->push(new DetachRootCommand(this, node, scenePos));
}

QJsonObject Document::toJson() const {
    QJsonObject o;
    o[QStringLiteral("version")] = 2;
    o[QStringLiteral("title")] = m_title;
    o[QStringLiteral("theme")] = m_theme.toJson();
    QJsonArray roots;
    for (const auto& root : m_roots)
        roots.append(root->toJson());
    o[QStringLiteral("roots")] = roots;
    QJsonArray conns;
    for (const auto& c : m_connections)
        conns.append(c.toJson());
    o[QStringLiteral("connections")] = conns;
    return o;
}

void Document::loadFromJson(const QJsonObject& o) {
    m_undo->clear();
    m_title = o.value(QStringLiteral("title")).toString(QStringLiteral("Untitled"));
    if (o.contains(QStringLiteral("theme")))
        m_theme = Theme::fromJson(o.value(QStringLiteral("theme")).toObject());
    m_roots.clear();
    if (o.contains(QStringLiteral("roots"))) {
        for (const auto& r : o.value(QStringLiteral("roots")).toArray())
            m_roots.push_back(Node::fromJson(r.toObject()));
    } else if (o.contains(QStringLiteral("root"))) { // v1 backward compatibility
        m_roots.push_back(Node::fromJson(o.value(QStringLiteral("root")).toObject()));
    }
    if (m_roots.empty())
        m_roots.push_back(std::make_unique<Node>(QStringLiteral("Central Idea")));
    m_connections.clear();
    for (const auto& c : o.value(QStringLiteral("connections")).toArray())
        m_connections.push_back(Connection::fromJson(c.toObject()));
    emit structureChanged();
}

} // namespace mindflow
