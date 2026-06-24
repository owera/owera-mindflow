// Reproduce the reported bug: fresh map, select root by MOUSE CLICK, Tab -> first
// child, type, Enter -> expected a sibling. Prints the resulting tree.
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

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1100, 760);
    w.show();
    QTest::qWaitForWindowExposed(&w);
    auto* view = w.findChild<MindMapView*>();
    auto* doc = w.findChild<Document*>();
    auto settle = [&] { for (int i = 0; i < 8; ++i) { app.processEvents(); QTest::qWait(20); } };
    settle();

    // Select the root by clicking it (no F2 first — mirrors the user's steps).
    QRectF rootRect;
    for (auto* it : view->scene()->items())
        if (it->type() == int(NodeItem::Type)) rootRect = it->sceneBoundingRect();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(rootRect.center()));
    settle();
    out << "selected after click: "
        << (view->scene()->selectedItems().isEmpty() ? "<none>" : "<a node>") << "\n";

    // Deliver keys to the viewport (the QGraphicsView focus proxy) to mimic how the
    // windowing system routes real keystrokes.
    QWidget* target = view->viewport();
    QTest::keyClick(target, Qt::Key_Tab); settle();   // add first child, edit it
    QTest::keyClicks(target, QStringLiteral("A")); settle();
    QTest::keyClick(target, Qt::Key_Return); settle(); // expected: add SIBLING of A
    QTest::keyClicks(target, QStringLiteral("B")); settle();
    view->scene()->setFocusItem(nullptr); settle();

    std::function<void(Node*, int)> dump = [&](Node* n, int d) {
        out << QString(d * 2, QLatin1Char(' ')) << "- '" << n->text() << "'\n";
        for (const auto& c : n->children()) dump(c.get(), d + 1);
    };
    out << "--- tree ---\n";
    dump(doc->root(), 0);

    Node* root = doc->root();
    const bool aIsChild = root->children().size() >= 1;
    const bool bIsSiblingOfA = root->children().size() == 2;
    out << (bIsSiblingOfA ? "PASS: B is sibling of A\n"
                          : "BUG: B is NOT a sibling of A (nested instead)\n");
    Q_UNUSED(aIsChild);
    return 0;
}
