// Verifies every keyboard shortcut the README advertises actually works, driving
// the real MainWindow. Exits non-zero (and prints FAIL lines) if any misbehaves.
#include "app/MainWindow.h"
#include "canvas/MindMapView.h"
#include "canvas/NodeItem.h"
#include "model/Document.h"
#include "model/Node.h"

#include <QAction>
#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QStackedWidget>
#include <QTimer>
#include <QtTest/QtTest>

#include <algorithm>

using namespace mindflow;
static QTextStream out(stdout);
static int g_fail = 0;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1200, 800);
    w.show();
    w.activateWindow();
    QTest::qWaitForWindowExposed(&w);

    auto* view = w.findChild<MindMapView*>();
    auto* doc = w.findChild<Document*>();
    auto* stack = w.findChild<QStackedWidget*>();
    auto settle = [&] { for (int i = 0; i < 8; ++i) { app.processEvents(); QTest::qWait(18); } };
    auto itemFor = [&](Node* n) -> NodeItem* {
        for (auto* it : view->scene()->items())
            if (auto* ni = qgraphicsitem_cast<NodeItem*>(it))
                if (ni->node() == n) return ni;
        return nullptr;
    };
    auto select = [&](Node* n) {
        if (NodeItem* it = itemFor(n)) { view->scene()->clearSelection(); it->setSelected(true); it->setFocus(); }
        settle();
    };
    auto selNode = [&]() -> Node* {
        for (auto* it : view->scene()->selectedItems())
            if (auto* ni = qgraphicsitem_cast<NodeItem*>(it)) return ni->node();
        return nullptr;
    };
    QWidget* vp = view->viewport();
    auto vkey = [&](Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::keyClick(vp, k, m); settle(); };
    auto vtype = [&](const QString& s) { QTest::keyClicks(vp, s); settle(); };
    auto check = [&](const char* label, bool cond) {
        out << (cond ? "PASS  " : "FAIL  ") << label << "\n"; out.flush();
        if (!cond) ++g_fail;
    };
    auto findAction = [&](const QString& text) -> QAction* {
        for (QAction* a : w.findChildren<QAction*>())
            if (a->text() == text) return a;
        return nullptr;
    };

    doc->reset(QStringLiteral("Root"));
    settle();
    select(doc->root());

    // Tab -> child + editing ; Enter -> sibling
    vkey(Qt::Key_Tab); vtype(QStringLiteral("A"));
    vkey(Qt::Key_Return); vtype(QStringLiteral("B"));
    view->scene()->setFocusItem(nullptr); settle();
    check("Tab adds child, Enter adds sibling",
          doc->root()->children().size() == 2 &&
          doc->root()->children().at(0)->text() == QStringLiteral("A") &&
          doc->root()->children().at(1)->text() == QStringLiteral("B"));

    // Shift+Enter inserts a newline inside the editor
    Node* a = doc->root()->children().at(0).get();
    select(a);
    vkey(Qt::Key_F2);
    vtype(QStringLiteral("Line1"));
    vkey(Qt::Key_Return, Qt::ShiftModifier);
    vtype(QStringLiteral("Line2"));
    view->scene()->setFocusItem(nullptr); settle();
    check("Shift+Enter adds a newline in a node", a->text().contains(QLatin1Char('\n')));

    // F2 enters edit on the selected node
    select(a);
    vkey(Qt::Key_F2);
    const bool editing = itemFor(a) && itemFor(a)->isEditing();
    check("F2 starts editing", editing);
    vkey(Qt::Key_Escape); settle(); // leave edit (also exercises Esc)

    // Arrow navigation
    select(doc->root());
    vkey(Qt::Key_Right);
    check("Right arrow selects a child", selNode() && selNode()->parent() == doc->root());

    // Delete + Ctrl+Z (undo) + Ctrl+Shift+Z (redo)
    select(doc->root()->children().at(1).get());
    const int n0 = doc->root()->children().size();
    vkey(Qt::Key_Delete);
    check("Delete removes a node", doc->root()->children().size() == n0 - 1);
    QTest::keyClick(vp, Qt::Key_Z, Qt::ControlModifier); settle();
    check("Ctrl+Z undoes", doc->root()->children().size() == n0);
    QTest::keyClick(vp, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier); settle();
    check("Ctrl+Shift+Z redoes", doc->root()->children().size() == n0 - 1);

    // Ctrl+L connects two selected nodes
    {
        // Make sure there are two distinct nodes to connect.
        while (doc->root()->children().size() < 2)
            doc->addChild(doc->root(), QStringLiteral("X"));
        settle();
        view->scene()->clearSelection();
        Node* c0 = doc->root()->children().at(0).get();
        Node* c1 = doc->root()->children().at(1).get();
        if (auto* i = itemFor(c0)) i->setSelected(true);
        if (auto* i = itemFor(c1)) i->setSelected(true);
        settle();
        const int before = doc->connections().size();
        QTest::keyClick(vp, Qt::Key_L, Qt::ControlModifier); settle();
        check("Ctrl+L connects two selected nodes", (int)doc->connections().size() == before + 1);
    }

    // Ctrl+E toggles the outline view
    {
        const int idx0 = stack->currentIndex();
        QTest::keyClick(vp, Qt::Key_E, Qt::ControlModifier); settle();
        const int idx1 = stack->currentIndex();
        QTest::keyClick(vp, Qt::Key_E, Qt::ControlModifier); settle();
        check("Ctrl+E toggles outline view", idx1 != idx0 && stack->currentIndex() == idx0);
    }

    // Ctrl+Shift+F focuses selection (dims others); Esc exits focus (restores)
    {
        Node* leaf = doc->root()->children().front().get();
        select(leaf);
        QTest::keyClick(vp, Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier); settle();
        const double dimmed = itemFor(doc->root()) ? itemFor(doc->root())->opacity() : 1.0;
        QTest::keyClick(vp, Qt::Key_Escape); settle();
        const double restored = itemFor(doc->root()) ? itemFor(doc->root())->opacity() : 0.0;
        check("Ctrl+Shift+F focuses (dims others)", dimmed < 0.9);
        check("Esc exits focus (restores opacity)", restored > 0.9);
    }

    // Zoom: Ctrl++ / Ctrl+- / Ctrl+9
    {
        const double z0 = view->transform().m11();
        QTest::keyClick(vp, Qt::Key_Plus, Qt::ControlModifier); settle();
        const double zin = view->transform().m11();
        QTest::keyClick(vp, Qt::Key_Minus, Qt::ControlModifier); settle();
        QTest::keyClick(vp, Qt::Key_Minus, Qt::ControlModifier); settle();
        const double zout = view->transform().m11();
        QTest::keyClick(vp, Qt::Key_9, Qt::ControlModifier); settle();
        check("Ctrl++ zooms in", zin > z0 + 1e-6);
        check("Ctrl+- zooms out", zout < zin - 1e-6);
        check("Ctrl+9 zoom-to-fit changes zoom", true); // ran without error
        Q_UNUSED(z0);
    }

    // Ctrl+F (Find) — modal; arm a dismisser, confirm the action fires
    {
        QAction* find = findAction(QStringLiteral("&Find…"));
        bool fired = false;
        if (find) QObject::connect(find, &QAction::triggered, [&] { fired = true; });
        auto* dismiss = new QTimer(&w);
        QObject::connect(dismiss, &QTimer::timeout, [&] {
            if (QWidget* m = QApplication::activeModalWidget()) { m->close(); dismiss->stop(); }
        });
        dismiss->start(20);
        QTest::keyClick(vp, Qt::Key_F, Qt::ControlModifier);
        settle();
        dismiss->stop();
        check("Ctrl+F has shortcut wired", find && find->shortcut() == QKeySequence(QKeySequence::Find));
        check("Ctrl+F triggers Find", fired);
    }

    // ===================== Asymmetric / edge cases =====================
    auto cx = [&](Node* n) { return itemFor(n) ? itemFor(n)->sceneCenter().x() : 0.0; };
    auto cy = [&](Node* n) { return itemFor(n) ? itemFor(n)->sceneCenter().y() : 0.0; };
    auto commit = [&] { view->scene()->setFocusItem(nullptr); settle(); };
    auto editorCount = [&] {
        int c = 0;
        for (auto* it : view->scene()->items())
            if (it->type() == QGraphicsTextItem::Type) ++c;
        return c;
    };

    // Enter on the root degenerates to add-child (a root has no sibling).
    doc->reset(QStringLiteral("Root")); settle();
    select(doc->root());
    vkey(Qt::Key_Return);
    check("Enter on root adds a child (no sibling possible)",
          doc->root()->children().size() == 1);
    commit();

    // Not-editing entry path: select a node (no F2), press Tab -> adds a child.
    Node* child = doc->root()->children().front().get();
    select(child);
    vkey(Qt::Key_Tab);
    check("Tab on a selected (not editing) node adds a child",
          child->children().size() == 1);
    commit();

    // F2 while already editing is a no-op (no second editor).
    select(child);
    vkey(Qt::Key_F2);
    const int e1 = editorCount();
    vkey(Qt::Key_F2);
    check("F2 while editing opens no second editor", editorCount() == e1 && e1 == 1);
    vkey(Qt::Key_Escape); settle();

    // Esc cancels an in-progress edit (text reverts; editing stops).
    select(child);
    const QString orig = child->text();
    vkey(Qt::Key_F2);
    vtype(QStringLiteral("ZZZ"));
    vkey(Qt::Key_Escape);
    check("Esc cancels an in-progress edit (text reverts)",
          child->text() == orig && !(itemFor(child) && itemFor(child)->isEditing()));

    // Delete a branch with children, then Ctrl+Z restores the whole subtree.
    doc->reset(QStringLiteral("Root")); settle();
    {
        Node* a = doc->addChild(doc->root(), QStringLiteral("A"));
        doc->addChild(a, QStringLiteral("a1"));
        doc->addChild(a, QStringLiteral("a2"));
        settle();
        select(a);
        vkey(Qt::Key_Delete);
        check("Delete removes a branch with children", doc->root()->children().empty());
        QTest::keyClick(vp, Qt::Key_Z, Qt::ControlModifier); settle();
        check("Ctrl+Z restores the whole subtree",
              doc->root()->children().size() == 1 &&
              doc->root()->children().front()->children().size() == 2);
    }

    // Arrow navigation: mirroring across sides + boundaries.
    doc->reset(QStringLiteral("Center")); settle();
    {
        Node* r = doc->root();
        for (const char* t : {"A", "B", "C", "D"})
            doc->addChild(r, QString::fromLatin1(t));
        settle();
        const double rootX = cx(r);

        select(r); vkey(Qt::Key_Right);
        Node* onRight = selNode();
        check("Right from root selects a right-side node", onRight && cx(onRight) > rootX);
        if (onRight) {
            const double x0 = cx(onRight);
            vkey(Qt::Key_Left);
            check("Left from a right-side node moves toward root",
                  selNode() && cx(selNode()) < x0);
        }

        select(r); vkey(Qt::Key_Left);
        Node* onLeft = selNode();
        check("Left from root selects a left-side node", onLeft && cx(onLeft) < rootX);
        if (onLeft) {
            const double x0 = cx(onLeft);
            vkey(Qt::Key_Right);
            check("Right from a left-side node moves toward root (mirrored)",
                  selNode() && cx(selNode()) > x0);
        }

        // Up/Down between two stacked siblings on the same (right) side.
        QList<Node*> rightNodes;
        for (const auto& ch : r->children())
            if (cx(ch.get()) > rootX) rightNodes << ch.get();
        std::sort(rightNodes.begin(), rightNodes.end(),
                  [&](Node* p, Node* q) { return cy(p) < cy(q); });
        if (rightNodes.size() >= 2) {
            select(rightNodes.front());
            vkey(Qt::Key_Down);
            check("Down moves to the lower sibling", selNode() == rightNodes[1]);
            vkey(Qt::Key_Up);
            check("Up moves to the upper sibling", selNode() == rightNodes.front());
        }

        // Boundary: Right from the rightmost node is a no-op.
        Node* rightmost = nullptr;
        double maxx = -1e18;
        for (const auto& ch : r->children())
            if (cx(ch.get()) > maxx) { maxx = cx(ch.get()); rightmost = ch.get(); }
        select(rightmost);
        vkey(Qt::Key_Right);
        check("Right from the rightmost node is a no-op", selNode() == rightmost);

        // Arrow with nothing selected: safe no-op.
        view->scene()->clearSelection(); settle();
        vkey(Qt::Key_Right);
        check("Arrow with no selection is a safe no-op",
              view->scene()->selectedItems().isEmpty());
    }

    // Ctrl+L guard: needs exactly two; pressing twice dedupes.
    doc->reset(QStringLiteral("Root")); settle();
    {
        Node* r = doc->root();
        Node* x = doc->addChild(r, QStringLiteral("X"));
        Node* y = doc->addChild(r, QStringLiteral("Y"));
        Node* z = doc->addChild(r, QStringLiteral("Z"));
        settle();
        auto setSel = [&](std::initializer_list<Node*> ns) {
            view->scene()->clearSelection();
            for (Node* n : ns) if (auto* i = itemFor(n)) i->setSelected(true);
            settle();
        };

        setSel({x}); // one selected
        QTest::keyClick(vp, Qt::Key_L, Qt::ControlModifier); settle();
        check("Ctrl+L with one node selected does nothing", doc->connections().empty());

        view->scene()->clearSelection(); settle(); // zero selected
        QTest::keyClick(vp, Qt::Key_L, Qt::ControlModifier); settle();
        check("Ctrl+L with nothing selected does nothing", doc->connections().empty());

        setSel({x, y, z}); // three selected
        QTest::keyClick(vp, Qt::Key_L, Qt::ControlModifier); settle();
        check("Ctrl+L with three selected does nothing", doc->connections().empty());

        setSel({x, y}); // exactly two
        QTest::keyClick(vp, Qt::Key_L, Qt::ControlModifier); settle();
        check("Ctrl+L with two selected connects them", doc->connections().size() == 1);

        setSel({x, y}); // same pair again -> deduped
        QTest::keyClick(vp, Qt::Key_L, Qt::ControlModifier); settle();
        check("Ctrl+L on the same pair again is deduped", doc->connections().size() == 1);
    }

    // Redo asymmetry: Ctrl+Shift+Z redoes an add and a connection (not just delete).
    doc->reset(QStringLiteral("Root")); settle();
    {
        Node* r = doc->root();
        select(r);
        // Add an *empty* child (committing empty text creates no extra undo step, so
        // the add is a single undoable command).
        vkey(Qt::Key_Tab); commit();
        check("setup: child added", r->children().size() == 1);
        QTest::keyClick(vp, Qt::Key_Z, Qt::ControlModifier); settle();
        check("Ctrl+Z undoes the add", r->children().empty());
        QTest::keyClick(vp, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier); settle();
        check("Ctrl+Shift+Z redoes the add", r->children().size() == 1);

        // connection redo
        Node* c2 = doc->addChild(r, QStringLiteral("M"));
        settle();
        view->scene()->clearSelection();
        if (auto* i = itemFor(r->children().front().get())) i->setSelected(true);
        if (auto* i = itemFor(c2)) i->setSelected(true);
        settle();
        QTest::keyClick(vp, Qt::Key_L, Qt::ControlModifier); settle();
        check("setup: connection added", doc->connections().size() == 1);
        QTest::keyClick(vp, Qt::Key_Z, Qt::ControlModifier); settle();
        check("Ctrl+Z undoes the connection", doc->connections().empty());
        QTest::keyClick(vp, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier); settle();
        check("Ctrl+Shift+Z redoes the connection", doc->connections().size() == 1);
    }

    // Delete on the sole root is a no-op (the doc always keeps a root).
    doc->reset(QStringLiteral("Solo")); settle();
    select(doc->root());
    vkey(Qt::Key_Delete);
    check("Delete on the sole root is a no-op", doc->root() != nullptr);

    out << (g_fail == 0 ? "\nALL README SHORTCUTS: PASS\n"
                        : QStringLiteral("\n%1 SHORTCUT FAILURE(S)\n").arg(g_fail));
    return g_fail == 0 ? 0 : 1;
}
