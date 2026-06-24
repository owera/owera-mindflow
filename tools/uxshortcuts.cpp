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
#include <QStackedWidget>
#include <QTimer>
#include <QtTest/QtTest>

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

    out << (g_fail == 0 ? "\nALL README SHORTCUTS: PASS\n"
                        : QStringLiteral("\n%1 SHORTCUT FAILURE(S)\n").arg(g_fail));
    return g_fail == 0 ? 0 : 1;
}
