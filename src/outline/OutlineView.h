#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace mindflow {

class Document;
class Node;

// A hierarchical outline of the same Document, always in sync with the Mind Map
// view. Editing text, toggling task checkboxes, and expanding/collapsing here all
// go through the same undoable Document operations the canvas uses, so the two
// views never diverge.
class OutlineView : public QWidget {
    Q_OBJECT
public:
    explicit OutlineView(QWidget* parent = nullptr);

    void setDocument(Document* doc);
    void selectNode(Node* node); // reflect external selection without echoing back

signals:
    void selectionChanged(mindflow::Node* node);

private:
    void rebuild();
    void buildItem(Node* node, QTreeWidgetItem* parentItem);
    Node* nodeFor(QTreeWidgetItem* item) const;
    QTreeWidgetItem* itemFor(int nodeId) const;

    void onItemChanged(QTreeWidgetItem* item, int column);
    void onSelectionChanged();
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void showContextMenu(const QPoint& pos);

    Document* m_doc = nullptr;
    QTreeWidget* m_tree = nullptr;
    bool m_updating = false; // guard: programmatic changes must not echo to model
};

} // namespace mindflow
