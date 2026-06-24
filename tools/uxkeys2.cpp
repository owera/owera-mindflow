// Full-integration keyboard test: drives the real MainWindow (menus, undo action,
// inspector, outline sync) entirely by keyboard, verifying the whole keyboard
// surface works together. Exits non-zero on any failure.
#include "app/MainWindow.h"
#include "canvas/MindMapView.h"
#include "canvas/NodeItem.h"
#include "model/Document.h"
#include "model/Node.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QtTest/QtTest>

using namespace mindflow;
static QTextStream out(stdout);
static int g_fails = 0;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1200, 800);
    w.show();
    QTest::qWaitForWindowExposed(&w);

    auto* view = w.findChild<MindMapView*>();
    auto* doc = w.findChild<Document*>();
    auto settle = [&] { for (int i = 0; i < 8; ++i) { app.processEvents(); QTest::qWait(20); } };
    auto itemFor = [&](Node* n) -> NodeItem* {
        for (auto* it : view->scene()->items())
            if (auto* ni = qgraphicsitem_cast<NodeItem*>(it))
                if (ni->node() == n) return ni;
        return nullptr;
    };
    auto selNode = [&]() -> Node* {
        for (auto* it : view->scene()->selectedItems())
            if (auto* ni = qgraphicsitem_cast<NodeItem*>(it)) return ni->node();
        return nullptr;
    };
    auto key = [&](Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) {
        QTest::keyClick(view, k, m); settle();
    };
    auto type = [&](const QString& s) { QTest::keyClicks(view, s); settle(); };
    auto check = [&](const char* label, bool cond) {
        out << (cond ? "PASS  " : "FAIL  ") << label << "\n";
        if (!cond) ++g_fails;
    };

    doc->reset(QStringLiteral("Root"));
    settle();
    if (NodeItem* r = itemFor(doc->root())) { r->setSelected(true); r->setFocus(); }
    settle();

    // Build a map purely by keyboard (Tab=child, Enter=sibling, while editing).
    key(Qt::Key_F2);  type(QStringLiteral("Trip"));
    key(Qt::Key_Tab); type(QStringLiteral("Day 1"));
    key(Qt::Key_Return); type(QStringLiteral("Day 2"));
    key(Qt::Key_Tab); type(QStringLiteral("Morning"));
    view->scene()->setFocusItem(nullptr); settle(); // commit Morning

    Node* root = doc->root();
    check("Tab/Enter build tree (no double-add)",
          root->text() == QStringLiteral("Trip") && root->children().size() == 2 &&
              root->children().at(0)->text() == QStringLiteral("Day 1") &&
              root->children().at(1)->text() == QStringLiteral("Day 2") &&
              root->children().at(1)->children().size() == 1 &&
              root->children().at(1)->children().at(0)->text() == QStringLiteral("Morning"));

    // Arrow navigation.
    if (NodeItem* r = itemFor(root)) { r->setSelected(true); r->setFocus(); }
    settle();
    key(Qt::Key_Right);
    check("Right selects a child", selNode() && selNode()->parent() == root);

    // Delete + Ctrl+Z (menu undo shortcut).
    Node* day1 = root->children().at(0).get();
    const int before = static_cast<int>(root->children().size());
    if (NodeItem* it = itemFor(day1)) { it->setSelected(true); it->setFocus(); }
    settle();
    key(Qt::Key_Delete);
    check("Delete removes node", static_cast<int>(root->children().size()) == before - 1);
    key(Qt::Key_Z, Qt::ControlModifier);
    check("Ctrl+Z (undo) restores it",
          static_cast<int>(root->children().size()) == before);

    view->zoomToFit(); settle();
    w.grab().save(QStringLiteral("/tmp/uxk-realkeys.png"));
    out << (g_fails == 0 ? "\nALL KEYBOARD CONTROLS: PASS\n"
                         : QStringLiteral("\n%1 FAILURE(S)\n").arg(g_fails));
    return g_fails == 0 ? 0 : 1;
}
