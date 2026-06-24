#include "model/Node.h"

#include <QJsonArray>

#include <algorithm>

namespace mindflow {

namespace {
// Monotonic id source. Persisted ids are honoured on load (see fromJson) and the
// counter is advanced past them so freshly created nodes never collide.
int g_nextId = 1;
int allocateId() { return g_nextId++; }
void bumpIdPast(int id) { g_nextId = std::max(g_nextId, id + 1); }
} // namespace

Node::Node(QString text) : m_id(allocateId()), m_text(std::move(text)) {}

void Node::setId(int id) {
    m_id = id;
    bumpIdPast(id);
}

int Node::indexInParent() const {
    if (!m_parent)
        return -1;
    const auto& siblings = m_parent->m_children;
    for (int i = 0; i < static_cast<int>(siblings.size()); ++i)
        if (siblings[i].get() == this)
            return i;
    return -1;
}

Node* Node::insertChild(std::unique_ptr<Node> child, int index) {
    child->setParent(this);
    Node* raw = child.get();
    if (index < 0 || index >= static_cast<int>(m_children.size()))
        m_children.push_back(std::move(child));
    else
        m_children.insert(m_children.begin() + index, std::move(child));
    return raw;
}

NodeContent Node::content() const {
    return NodeContent{note, tags, sticker, isTask, taskDone, imagePng};
}

void Node::setContent(const NodeContent& c) {
    note = c.note;
    tags = c.tags;
    sticker = c.sticker;
    isTask = c.isTask;
    taskDone = c.taskDone;
    imagePng = c.imagePng;
}

void Node::taskProgress(int& done, int& total) const {
    if (isTask) {
        ++total;
        if (taskDone)
            ++done;
    }
    for (const auto& c : m_children)
        c->taskProgress(done, total);
}

std::unique_ptr<Node> Node::takeChild(Node* child) {
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (it->get() == child) {
            std::unique_ptr<Node> owned = std::move(*it);
            m_children.erase(it);
            owned->setParent(nullptr);
            return owned;
        }
    }
    return nullptr;
}

QJsonObject Node::toJson() const {
    QJsonObject o;
    o[QStringLiteral("id")] = m_id;
    o[QStringLiteral("text")] = m_text;
    o[QStringLiteral("style")] = style.toJson();
    o[QStringLiteral("layout")] = mindflow::toString(layoutDirection);
    o[QStringLiteral("x")] = layoutPos.x();
    o[QStringLiteral("y")] = layoutPos.y();
    o[QStringLiteral("manualPos")] = hasManualPos;
    o[QStringLiteral("collapsed")] = collapsed;
    if (!note.isEmpty())
        o[QStringLiteral("note")] = note;
    if (!tags.isEmpty())
        o[QStringLiteral("tags")] = QJsonArray::fromStringList(tags);
    if (!sticker.isEmpty())
        o[QStringLiteral("sticker")] = sticker;
    if (isTask) {
        o[QStringLiteral("task")] = true;
        o[QStringLiteral("done")] = taskDone;
    }
    if (!imagePng.isEmpty())
        o[QStringLiteral("image")] = QString::fromLatin1(imagePng.toBase64());

    QJsonArray kids;
    for (const auto& c : m_children)
        kids.append(c->toJson());
    o[QStringLiteral("children")] = kids;
    return o;
}

std::unique_ptr<Node> Node::fromJson(const QJsonObject& o) {
    auto node = std::make_unique<Node>(o.value(QStringLiteral("text")).toString());
    if (o.contains(QStringLiteral("id")))
        node->setId(o.value(QStringLiteral("id")).toInt());
    node->style = NodeStyle::fromJson(o.value(QStringLiteral("style")).toObject());
    node->layoutDirection =
        layoutDirectionFromString(o.value(QStringLiteral("layout")).toString());
    node->layoutPos = QPointF(o.value(QStringLiteral("x")).toDouble(),
                              o.value(QStringLiteral("y")).toDouble());
    node->hasManualPos = o.value(QStringLiteral("manualPos")).toBool();
    node->collapsed = o.value(QStringLiteral("collapsed")).toBool();
    node->note = o.value(QStringLiteral("note")).toString();
    for (const auto& t : o.value(QStringLiteral("tags")).toArray())
        node->tags << t.toString();
    node->sticker = o.value(QStringLiteral("sticker")).toString();
    node->isTask = o.value(QStringLiteral("task")).toBool();
    node->taskDone = o.value(QStringLiteral("done")).toBool();
    const QString img = o.value(QStringLiteral("image")).toString();
    if (!img.isEmpty())
        node->imagePng = QByteArray::fromBase64(img.toLatin1());

    for (const auto& c : o.value(QStringLiteral("children")).toArray())
        node->insertChild(Node::fromJson(c.toObject()));
    return node;
}

} // namespace mindflow
