#include "outline/OutlineView.h"

#include "model/Document.h"
#include "model/Node.h"

#include <QHeaderView>
#include <QMenu>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace mindflow {

namespace {
constexpr int kNodeIdRole = Qt::UserRole + 1;
}

OutlineView::OutlineView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked |
                            QAbstractItemView::EditKeyPressed);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setUniformRowHeights(true);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemChanged, this, &OutlineView::onItemChanged);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this,
            &OutlineView::onSelectionChanged);
    connect(m_tree, &QTreeWidget::itemExpanded, this, &OutlineView::onItemExpanded);
    connect(m_tree, &QTreeWidget::itemCollapsed, this, &OutlineView::onItemCollapsed);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
            &OutlineView::showContextMenu);
}

void OutlineView::setDocument(Document* doc) {
    if (m_doc)
        m_doc->disconnect(this);
    m_doc = doc;
    if (m_doc) {
        connect(m_doc, &Document::structureChanged, this, &OutlineView::rebuild);
        connect(m_doc, &Document::nodeChanged, this, [this](Node*) { rebuild(); });
    }
    rebuild();
}

Node* OutlineView::nodeFor(QTreeWidgetItem* item) const {
    if (!item || !m_doc)
        return nullptr;
    return m_doc->nodeById(item->data(0, kNodeIdRole).toInt());
}

QTreeWidgetItem* OutlineView::itemFor(int nodeId) const {
    QTreeWidgetItemIterator it(m_tree);
    for (; *it; ++it)
        if ((*it)->data(0, kNodeIdRole).toInt() == nodeId)
            return *it;
    return nullptr;
}

void OutlineView::buildItem(Node* node, QTreeWidgetItem* parentItem) {
    auto* item = parentItem ? new QTreeWidgetItem(parentItem)
                            : new QTreeWidgetItem(m_tree);
    item->setData(0, kNodeIdRole, node->id());

    QString text = node->text();
    if (!node->sticker.isEmpty())
        text = node->sticker + QLatin1Char(' ') + text;
    item->setText(0, text);

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    if (node->isTask) {
        flags |= Qt::ItemIsUserCheckable;
        item->setCheckState(0, node->taskDone ? Qt::Checked : Qt::Unchecked);
    }
    item->setFlags(flags);

    // Surface the rest of the rich content as a tooltip.
    QStringList tip;
    if (!node->tags.isEmpty())
        tip << tr("Tags: %1").arg(node->tags.join(QStringLiteral(", ")));
    if (!node->note.isEmpty())
        tip << tr("Note: %1").arg(node->note);
    if (!node->imagePng.isEmpty())
        tip << tr("[image]");
    if (!tip.isEmpty())
        item->setToolTip(0, tip.join(QLatin1Char('\n')));

    for (const auto& c : node->children())
        buildItem(c.get(), item);

    item->setExpanded(!node->collapsed);
}

void OutlineView::rebuild() {
    if (!m_doc)
        return;
    const int selectedId =
        m_tree->currentItem() ? m_tree->currentItem()->data(0, kNodeIdRole).toInt() : -1;

    m_updating = true;
    m_tree->clear();
    for (const auto& root : m_doc->roots())
        buildItem(root.get(), nullptr);
    if (selectedId >= 0) {
        if (QTreeWidgetItem* it = itemFor(selectedId))
            m_tree->setCurrentItem(it);
    }
    m_updating = false;
}

void OutlineView::onItemChanged(QTreeWidgetItem* item, int) {
    if (m_updating || !m_doc)
        return;
    Node* node = nodeFor(item);
    if (!node)
        return;

    // Task checkbox toggled?
    if (node->isTask) {
        const bool desired = item->checkState(0) == Qt::Checked;
        if (desired != node->taskDone) {
            m_doc->toggleTaskDone(node);
            return; // rebuild will follow
        }
    }
    // Text edited? Strip the sticker prefix we prepend for display.
    QString edited = item->text(0);
    if (!node->sticker.isEmpty()) {
        const QString prefix = node->sticker + QLatin1Char(' ');
        if (edited.startsWith(prefix))
            edited = edited.mid(prefix.size());
    }
    if (edited != node->text())
        m_doc->setNodeText(node, edited);
}

void OutlineView::onSelectionChanged() {
    if (m_updating)
        return;
    emit selectionChanged(nodeFor(m_tree->currentItem()));
}

void OutlineView::onItemExpanded(QTreeWidgetItem* item) {
    if (m_updating || !m_doc)
        return;
    if (Node* node = nodeFor(item))
        if (node->collapsed)
            m_doc->setCollapsed(node, false);
}

void OutlineView::onItemCollapsed(QTreeWidgetItem* item) {
    if (m_updating || !m_doc)
        return;
    if (Node* node = nodeFor(item))
        if (!node->collapsed)
            m_doc->setCollapsed(node, true);
}

void OutlineView::selectNode(Node* node) {
    if (!node)
        return;
    m_updating = true;
    if (QTreeWidgetItem* it = itemFor(node->id())) {
        m_tree->setCurrentItem(it);
        m_tree->scrollToItem(it);
    }
    m_updating = false;
}

void OutlineView::showContextMenu(const QPoint& pos) {
    if (!m_doc)
        return;
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    Node* node = nodeFor(item);

    QMenu menu(this);
    menu.addAction(tr("Add Child"), [this, node] {
        Node* parent = node ? node : m_doc->root();
        m_doc->addChild(parent);
    });
    if (node && node->parent()) {
        menu.addAction(tr("Add Sibling"), [this, node] { m_doc->addSibling(node); });
    }
    if (node) {
        menu.addSeparator();
        menu.addAction(tr("Delete"), [this, node] { m_doc->removeNode(node); });
    }
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

} // namespace mindflow
