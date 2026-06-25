// Double-click-to-edit + the move-commit regression behind it: a plain click must
// NOT push a "Move Node" command (which would rebuild the scene and destroy the
// editor a double-click just opened); a real drag still commits a move.
#include "app/MainWindow.h"
#include "canvas/MindMapView.h"
#include "canvas/NodeItem.h"
#include "model/Document.h"
#include "model/Node.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QUndoStack>
#include <QtTest/QtTest>

using namespace mindflow;
static QTextStream out(stdout);
static int g_fail = 0;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1100, 760);
    w.show();
    w.activateWindow();
    QTest::qWaitForWindowExposed(&w);

    auto* view = w.findChild<MindMapView*>();
    auto* doc = w.findChild<Document*>();
    auto* vp = view->viewport();
    auto settle = [&] { for (int i = 0; i < 8; ++i) { app.processEvents(); QTest::qWait(18); } };
    auto itemFor = [&](Node* n) -> NodeItem* {
        for (auto* it : view->scene()->items())
            if (auto* ni = qgraphicsitem_cast<NodeItem*>(it))
                if (ni->node() == n) return ni;
        return nullptr;
    };
    auto vpos = [&](Node* n) { return view->mapFromScene(itemFor(n)->sceneCenter()); };
    auto check = [&](const char* label, bool cond) {
        out << (cond ? "PASS  " : "FAIL  ") << label << "\n";
        if (!cond) ++g_fail;
    };

    doc->reset(QStringLiteral("Hello"));
    settle();
    Node* root = doc->root();

    // A plain click must not push any command (no spurious "Move Node").
    check("clean undo stack after reset", !doc->undoStack()->canUndo());
    QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier, vpos(root));
    settle();
    check("a plain click pushes no Move command", !doc->undoStack()->canUndo());

    // Double-click opens the inline editor.
    QTest::mouseDClick(vp, Qt::LeftButton, Qt::NoModifier, vpos(root));
    settle();
    check("double-click edits the node", itemFor(root) && itemFor(root)->isEditing());
    // commit and make sure that itself didn't leave a stray move
    view->scene()->setFocusItem(nullptr);
    settle();

    // A real drag DOES commit a move.
    doc->reset(QStringLiteral("Drag me"));
    settle();
    Node* r2 = doc->root();
    const QPoint from = vpos(r2);
    const QPoint to = from + QPoint(120, 40);
    QTest::mousePress(vp, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(vp, from + QPoint(40, 10)); settle();
    QTest::mouseMove(vp, to); settle();
    QTest::mouseRelease(vp, Qt::LeftButton, Qt::NoModifier, to);
    settle();
    check("a real drag commits a Move", doc->undoStack()->canUndo() && r2->hasManualPos);

    out << (g_fail == 0 ? "\nDOUBLE-CLICK / DRAG: PASS\n"
                        : QStringLiteral("\n%1 FAILURE(S)\n").arg(g_fail));
    return g_fail == 0 ? 0 : 1;
}
