#include "io/DocumentStore.h"
#include "io/Exporters.h"
#include "io/Importers.h"
#include "layout/LayoutEngine.h"
#include "model/Document.h"
#include "model/Node.h"
#include "model/Theme.h"

#include <QBuffer>
#include <QImage>
#include <QSizeF>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QtTest/QtTest>

using namespace mindflow;

class TestModel : public QObject {
    Q_OBJECT
private slots:
    void addAndUndo();
    void editTextMerges();
    void removeAndUndoKeepsSubtree();
    void moveReparents();
    void layoutIsDeterministicAndSeparates();
    void verticalLayoutStacksChildrenHorizontally();
    void collapseHidesSubtreeFromLayout();
    void detachCreatesRootAndUndoes();
    void saveLoadRoundTrip();
    void multiRootRoundTrip();
    void branchesGetDistinctColors();
    void styleChangeIsUndoable();
    void applyThemeRecolorsAndUndoes();
    void taskProgressCountsSubtree();
    void contentRoundTripWithImage();
    void allTagsAreCollected();
    void connectionAddRemoveUndo();
    void connectionRoundTrip();
    void addSiblingOnRootFallsBackToChild();
    void removeSoleRootIsNoOp();
    void markdownRoundTrip();
    void opmlRoundTrip();
    void markdownImportsTasksAndTags();
};

void TestModel::addAndUndo() {
    Document doc;
    Node* root = doc.root();
    QCOMPARE(root->children().size(), size_t(0));

    Node* a = doc.addChild(root, QStringLiteral("A"));
    QVERIFY(a != nullptr);
    QCOMPARE(root->children().size(), size_t(1));
    QCOMPARE(a->parent(), root);

    doc.undoStack()->undo();
    QCOMPARE(root->children().size(), size_t(0));
    doc.undoStack()->redo();
    QCOMPARE(root->children().size(), size_t(1));
}

void TestModel::editTextMerges() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("x"));
    const int baseIndex = doc.undoStack()->index();

    doc.setNodeText(a, QStringLiteral("he"));
    doc.setNodeText(a, QStringLiteral("hel"));
    doc.setNodeText(a, QStringLiteral("hello"));
    QCOMPARE(a->text(), QStringLiteral("hello"));

    // The three keystroke edits should merge into a single undo step.
    QCOMPARE(doc.undoStack()->index(), baseIndex + 1);
    doc.undoStack()->undo();
    QCOMPARE(a->text(), QStringLiteral("x"));
}

void TestModel::removeAndUndoKeepsSubtree() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
    Node* b = doc.addChild(a, QStringLiteral("B"));
    const int bId = b->id();

    doc.removeNode(a);
    QCOMPARE(doc.root()->children().size(), size_t(0));

    doc.undoStack()->undo();
    QCOMPARE(doc.root()->children().size(), size_t(1));
    Node* restored = doc.root()->children().front().get();
    QCOMPARE(restored->children().size(), size_t(1));
    QCOMPARE(restored->children().front()->id(), bId); // subtree survived intact
}

void TestModel::moveReparents() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
    Node* b = doc.addChild(doc.root(), QStringLiteral("B"));
    Node* c = doc.addChild(a, QStringLiteral("C"));

    doc.moveNode(c, b, -1, QPointF(10, 10));
    QCOMPARE(c->parent(), b);
    QCOMPARE(a->children().size(), size_t(0));
    QCOMPARE(b->children().size(), size_t(1));

    doc.undoStack()->undo();
    QCOMPARE(c->parent(), a);
}

void TestModel::layoutIsDeterministicAndSeparates() {
    Document doc;
    Node* root = doc.root();
    Node* a = doc.addChild(root, QStringLiteral("A"));
    Node* b = doc.addChild(root, QStringLiteral("B"));

    LayoutEngine engine;
    auto sizeOf = [](const Node*) { return QSizeF(100, 40); };
    engine.layout(root, sizeOf);

    // Root centered at origin; the two children land on opposite sides (balanced).
    QCOMPARE(root->layoutPos, QPointF(0, 0));
    QVERIFY(a->layoutPos.x() != b->layoutPos.x());
    QVERIFY((a->layoutPos.x() > 0) != (b->layoutPos.x() > 0));

    // Deterministic: a second pass yields identical coordinates.
    const QPointF aPos = a->layoutPos;
    engine.layout(root, sizeOf);
    QCOMPARE(a->layoutPos, aPos);
}

void TestModel::verticalLayoutStacksChildrenHorizontally() {
    Document doc;
    Node* root = doc.root();
    root->layoutDirection = LayoutDirection::Down;
    Node* a = doc.addChild(root, QStringLiteral("A"));
    Node* b = doc.addChild(root, QStringLiteral("B"));

    LayoutEngine engine;
    auto sizeOf = [](const Node*) { return QSizeF(100, 40); };
    engine.layout(root, sizeOf);

    // Down layout: children sit below the root (larger y) and spread along x.
    QVERIFY(a->layoutPos.y() > root->layoutPos.y());
    QVERIFY(b->layoutPos.y() > root->layoutPos.y());
    QVERIFY(a->layoutPos.x() != b->layoutPos.x());
    QCOMPARE(a->layoutPos.y(), b->layoutPos.y()); // same depth level
}

void TestModel::collapseHidesSubtreeFromLayout() {
    Document doc;
    Node* root = doc.root();
    Node* a = doc.addChild(root, QStringLiteral("A"));
    Node* child = doc.addChild(a, QStringLiteral("child"));

    LayoutEngine engine;
    auto sizeOf = [](const Node*) { return QSizeF(100, 40); };

    engine.layout(root, sizeOf);
    const QPointF expanded = child->layoutPos;

    doc.setCollapsed(a, true);
    QVERIFY(a->collapsed);
    // Collapsing must be undoable.
    doc.undoStack()->undo();
    QVERIFY(!a->collapsed);
    doc.undoStack()->redo();
    QVERIFY(a->collapsed);

    // A collapsed node's children are not repositioned by the engine.
    child->layoutPos = QPointF(-999, -999);
    engine.layout(root, sizeOf);
    QCOMPARE(child->layoutPos, QPointF(-999, -999));
    Q_UNUSED(expanded);
}

void TestModel::detachCreatesRootAndUndoes() {
    Document doc;
    Node* root = doc.root();
    Node* a = doc.addChild(root, QStringLiteral("A"));
    Node* b = doc.addChild(a, QStringLiteral("B"));
    const int bId = b->id();

    QCOMPARE(doc.roots().size(), size_t(1));
    doc.detachAsRoot(b, QPointF(200, 50));
    QCOMPARE(doc.roots().size(), size_t(2));
    QVERIFY(doc.isRoot(b));
    QCOMPARE(b->parent(), nullptr);
    QCOMPARE(a->children().size(), size_t(0));
    QVERIFY(b->hasManualPos);

    doc.undoStack()->undo();
    QCOMPARE(doc.roots().size(), size_t(1));
    Node* restored = doc.nodeById(bId);
    QVERIFY(restored != nullptr);
    QCOMPARE(restored->parent(), a);
}

void TestModel::saveLoadRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("map.mindflow"));

    int childId = 0;
    {
        Document doc;
        doc.setTitle(QStringLiteral("My Map"));
        Node* a = doc.addChild(doc.root(), QStringLiteral("Alpha"));
        doc.addChild(a, QStringLiteral("Beta"));
        childId = a->id();
        QString err;
        QVERIFY2(DocumentStore::save(doc, path, &err), qPrintable(err));
    }
    {
        Document doc;
        QString err;
        QVERIFY2(DocumentStore::load(doc, path, &err), qPrintable(err));
        QCOMPARE(doc.title(), QStringLiteral("My Map"));
        QCOMPARE(doc.root()->children().size(), size_t(1));
        Node* a = doc.root()->children().front().get();
        QCOMPARE(a->text(), QStringLiteral("Alpha"));
        QCOMPARE(a->id(), childId); // ids are preserved across save/load
        QCOMPARE(a->children().front()->text(), QStringLiteral("Beta"));
    }
}

void TestModel::multiRootRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("multi.mindflow"));
    {
        Document doc;
        Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
        doc.root()->layoutDirection = LayoutDirection::Down;
        doc.detachAsRoot(a, QPointF(300, 80));
        QCOMPARE(doc.roots().size(), size_t(2));
        QString err;
        QVERIFY2(DocumentStore::save(doc, path, &err), qPrintable(err));
    }
    {
        Document doc;
        QString err;
        QVERIFY2(DocumentStore::load(doc, path, &err), qPrintable(err));
        QCOMPARE(doc.roots().size(), size_t(2));
        QVERIFY(doc.root()->layoutDirection == LayoutDirection::Down);
    }
}

void TestModel::branchesGetDistinctColors() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
    Node* b = doc.addChild(doc.root(), QStringLiteral("B"));
    Node* aChild = doc.addChild(a, QStringLiteral("a1"));

    // Two top-level branches use different palette colors.
    QVERIFY(a->style.fillColor != b->style.fillColor);
    // A descendant inherits its branch color.
    QCOMPARE(aChild->style.fillColor, a->style.fillColor);
}

void TestModel::styleChangeIsUndoable() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
    const NodeShape original = a->style.shape;

    NodeStyle s = a->style;
    s.shape = NodeShape::Hexagon;
    doc.setNodeStyle(a, s);
    QVERIFY(a->style.shape == NodeShape::Hexagon);

    doc.undoStack()->undo();
    QVERIFY(a->style.shape == original);
}

void TestModel::applyThemeRecolorsAndUndoes() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
    const QColor before = a->style.fillColor;

    const Theme midnight = Theme::byName(QStringLiteral("Midnight"));
    doc.applyTheme(midnight);
    QCOMPARE(doc.root()->style.fillColor, midnight.rootFill);
    QCOMPARE(a->style.fillColor, midnight.branchColor(0));

    doc.undoStack()->undo();
    QCOMPARE(a->style.fillColor, before);
}

void TestModel::taskProgressCountsSubtree() {
    Document doc;
    Node* root = doc.root();
    Node* a = doc.addChild(root, QStringLiteral("A"));
    Node* b = doc.addChild(root, QStringLiteral("B"));

    NodeContent ca = a->content();
    ca.isTask = true;
    ca.taskDone = true;
    doc.setNodeContent(a, ca);
    NodeContent cb = b->content();
    cb.isTask = true; // not done
    doc.setNodeContent(b, cb);

    int done = 0, total = 0;
    root->taskProgress(done, total);
    QCOMPARE(total, 2);
    QCOMPARE(done, 1);

    doc.toggleTaskDone(b);
    done = total = 0;
    root->taskProgress(done, total);
    QCOMPARE(done, 2);
}

void TestModel::contentRoundTripWithImage() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("content.mindflow"));

    QByteArray png;
    {
        QImage img(8, 8, QImage::Format_ARGB32);
        img.fill(Qt::red);
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
    }
    {
        Document doc;
        Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
        NodeContent c = a->content();
        c.note = QStringLiteral("a note");
        c.tags = {QStringLiteral("x"), QStringLiteral("y")};
        c.sticker = QStringLiteral("⭐");
        c.isTask = true;
        c.taskDone = true;
        c.imagePng = png;
        doc.setNodeContent(a, c);
        QString err;
        QVERIFY2(DocumentStore::save(doc, path, &err), qPrintable(err));
    }
    {
        Document doc;
        QString err;
        QVERIFY2(DocumentStore::load(doc, path, &err), qPrintable(err));
        Node* a = doc.root()->children().front().get();
        QCOMPARE(a->note, QStringLiteral("a note"));
        QCOMPARE(a->tags.size(), 2);
        QCOMPARE(a->sticker, QStringLiteral("⭐"));
        QVERIFY(a->isTask && a->taskDone);
        QCOMPARE(a->imagePng, png); // image survives base64 round-trip
    }
}

void TestModel::allTagsAreCollected() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
    Node* b = doc.addChild(doc.root(), QStringLiteral("B"));
    NodeContent ca = a->content();
    ca.tags = {QStringLiteral("red"), QStringLiteral("blue")};
    doc.setNodeContent(a, ca);
    NodeContent cb = b->content();
    cb.tags = {QStringLiteral("blue"), QStringLiteral("green")};
    doc.setNodeContent(b, cb);

    const QStringList tags = doc.allTags();
    QCOMPARE(tags.size(), 3); // red, blue, green (deduped)
    QVERIFY(tags.contains(QStringLiteral("blue")));
}

void TestModel::connectionAddRemoveUndo() {
    Document doc;
    Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
    Node* b = doc.addChild(doc.root(), QStringLiteral("B"));

    doc.addConnection(a, b);
    QCOMPARE(doc.connections().size(), size_t(1));
    // Duplicate (either direction) is ignored.
    doc.addConnection(b, a);
    QCOMPARE(doc.connections().size(), size_t(1));

    const int id = doc.connections().front().id;
    doc.setConnectionLabel(id, QStringLiteral("relates to"));
    QCOMPARE(doc.connectionById(id)->label, QStringLiteral("relates to"));

    doc.undoStack()->undo(); // undo label
    QVERIFY(doc.connectionById(id)->label.isEmpty());

    doc.removeConnection(id);
    QCOMPARE(doc.connections().size(), size_t(0));
    doc.undoStack()->undo(); // undo remove
    QCOMPARE(doc.connections().size(), size_t(1));
}

void TestModel::connectionRoundTrip() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("conn.mindflow"));
    int connId = 0;
    {
        Document doc;
        Node* a = doc.addChild(doc.root(), QStringLiteral("A"));
        Node* b = doc.addChild(doc.root(), QStringLiteral("B"));
        doc.addConnection(a, b);
        connId = doc.connections().front().id;
        doc.setConnectionLabel(connId, QStringLiteral("link"));
        doc.setConnectionWaypoint(connId, QPointF(15, -20));
        QString err;
        QVERIFY2(DocumentStore::save(doc, path, &err), qPrintable(err));
    }
    {
        Document doc;
        QString err;
        QVERIFY2(DocumentStore::load(doc, path, &err), qPrintable(err));
        QCOMPARE(doc.connections().size(), size_t(1));
        const Connection& c = doc.connections().front();
        QCOMPARE(c.label, QStringLiteral("link"));
        QCOMPARE(c.waypointOffset, QPointF(15, -20));
    }
}

void TestModel::markdownRoundTrip() {
    Document src;
    src.setNodeText(src.root(), QStringLiteral("Root"));
    Node* a = src.addChild(src.root(), QStringLiteral("Alpha"));
    src.addChild(a, QStringLiteral("Alpha One"));
    src.addChild(src.root(), QStringLiteral("Beta"));

    const QString md = exporters::toMarkdown(src);
    auto roots = importers::fromMarkdown(md);

    Document dst;
    dst.setImportedRoots(std::move(roots), QStringLiteral("Imported"));
    QCOMPARE(dst.root()->text(), QStringLiteral("Root"));
    QCOMPARE(dst.root()->children().size(), size_t(2));
    Node* alpha = dst.root()->children().front().get();
    QCOMPARE(alpha->text(), QStringLiteral("Alpha"));
    QCOMPARE(alpha->children().size(), size_t(1));
    QCOMPARE(alpha->children().front()->text(), QStringLiteral("Alpha One"));
}

void TestModel::opmlRoundTrip() {
    Document src;
    src.setNodeText(src.root(), QStringLiteral("Plan"));
    Node* a = src.addChild(src.root(), QStringLiteral("Phase 1"));
    src.addChild(a, QStringLiteral("Task A"));

    const QString opml = exporters::toOpml(src);
    auto roots = importers::fromOpml(opml);

    Document dst;
    dst.setImportedRoots(std::move(roots), QStringLiteral("Imported"));
    QCOMPARE(dst.root()->text(), QStringLiteral("Plan"));
    QCOMPARE(dst.root()->children().front()->text(), QStringLiteral("Phase 1"));
    QCOMPARE(dst.root()->children().front()->children().front()->text(),
             QStringLiteral("Task A"));
}

void TestModel::markdownImportsTasksAndTags() {
    const QString md = QStringLiteral(
        "# Project\n- [x] Done item #urgent\n- [ ] Todo item\n  - Subtask\n");
    auto roots = importers::fromMarkdown(md);
    Document dst;
    dst.setImportedRoots(std::move(roots), QStringLiteral("P"));

    QCOMPARE(dst.root()->text(), QStringLiteral("Project"));
    Node* done = dst.root()->children().front().get();
    QCOMPARE(done->text(), QStringLiteral("Done item"));
    QVERIFY(done->isTask && done->taskDone);
    QVERIFY(done->tags.contains(QStringLiteral("urgent")));

    Node* todo = dst.root()->children().at(1).get();
    QVERIFY(todo->isTask && !todo->taskDone);
    QCOMPARE(todo->children().front()->text(), QStringLiteral("Subtask"));
}

void TestModel::addSiblingOnRootFallsBackToChild() {
    // A root has no parent, so "add sibling" degenerates to "add child".
    Document doc;
    Node* root = doc.root();
    Node* added = doc.addSibling(root, QStringLiteral("X"));
    QVERIFY(added != nullptr);
    QCOMPARE(added->parent(), root);
    QCOMPARE(root->children().size(), size_t(1));
}

void TestModel::removeSoleRootIsNoOp() {
    // The document must always keep at least one root; deleting the only root is a
    // no-op (and must not crash).
    Document doc;
    Node* root = doc.root();
    doc.removeNode(root);
    QCOMPARE(doc.roots().size(), size_t(1));
    QCOMPARE(doc.root(), root);
    QVERIFY(!doc.undoStack()->canUndo()); // nothing was pushed
}

QTEST_MAIN(TestModel)
#include "test_model.moc"
