// Deterministic keyboard UX test. Delivers key events straight to the scene's
// focus item (bypassing headless window-activation flakiness) so the keyboard
// construction/navigation controls can be verified repeatably. Exits non-zero if
// any control misbehaves, printing a PASS/FAIL line per check.
#include "app/MainWindow.h"
#include "canvas/MindMapView.h"
#include "canvas/NodeItem.h"
#include "model/Document.h"
#include "model/Node.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QString>
#include <QtTest/QtTest>

using namespace mindflow;

static QTextStream out(stdout);
static int g_failures = 0;

static void settle(QApplication& app) {
    for (int i = 0; i < 6; ++i) {
        app.processEvents();
        QTest::qWait(15);
    }
}

static void check(const QString& label, bool ok, const QString& detail = QString()) {
    out << (ok ? "PASS  " : "FAIL  ") << label;
    if (!detail.isEmpty())
        out << "  [" << detail << "]";
    out << "\n";
    out.flush();
    if (!ok)
        ++g_failures;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1200, 800);
    w.show();
    settle(app);

    auto* view = w.findChild<MindMapView*>();
    auto* doc = w.findChild<Document*>();
    auto* scene = view->scene();

    auto itemFor = [&](Node* n) -> NodeItem* {
        const auto items = scene->items();
        for (auto* it : items)
            if (auto* ni = qgraphicsitem_cast<NodeItem*>(it))
                if (ni->node() == n)
                    return ni;
        return nullptr;
    };
    auto selectedNode = [&]() -> Node* {
        for (auto* it : scene->selectedItems())
            if (auto* ni = qgraphicsitem_cast<NodeItem*>(it))
                return ni->node();
        return nullptr;
    };
    auto selectedText = [&]() -> QString {
        Node* n = selectedNode();
        return n ? n->text() : QStringLiteral("<none>");
    };

    // Deliver a key to whatever item currently holds scene focus.
    auto sendKey = [&](Qt::Key k, Qt::KeyboardModifiers mods = Qt::NoModifier,
                       const QString& text = QString()) {
        QKeyEvent press(QEvent::KeyPress, k, mods, text);
        QApplication::sendEvent(scene, &press);
        QKeyEvent release(QEvent::KeyRelease, k, mods, text);
        QApplication::sendEvent(scene, &release);
        settle(app);
    };
    // Focus a node as the scene focus item, then optionally select it.
    auto focusNode = [&](Node* n) {
        NodeItem* it = itemFor(n);
        if (!it)
            return;
        scene->clearSelection();
        it->setSelected(true);
        scene->setFocusItem(it);
        settle(app);
    };
    auto typeText = [&](const QString& s) {
        for (const QChar ch : s)
            sendKey(Qt::Key_unknown, Qt::NoModifier, QString(ch));
    };

    // Begin from a clean, known document.
    doc->reset(QStringLiteral("Roadmap"));
    settle(app);
    focusNode(doc->root());

    // 1. F2 rename: F2, type, Enter -> text changes, node stays selected.
    sendKey(Qt::Key_F2);
    typeText(QStringLiteral("Product"));
    sendKey(Qt::Key_Return);
    check(QStringLiteral("F2 rename root"), doc->root()->text() == QStringLiteral("Product"),
          doc->root()->text());
    check(QStringLiteral("focus retained after Enter-commit"),
          selectedNode() == doc->root(), selectedText());

    // 2. Tab adds a child and opens its editor; typing names it.
    sendKey(Qt::Key_Tab);
    {
        QGraphicsItem* fi = scene->focusItem();
        out << "   [dbg] post-Tab focusItem type=" << (fi ? fi->type() : -1)
            << " isText=" << (fi && fi->type() == 8 /*QGraphicsTextItem*/) << "\n";
        out.flush();
    }
    typeText(QStringLiteral("Marketing"));
    {
        Node* c = doc->root()->children().empty() ? nullptr
                                                   : doc->root()->children().front().get();
        out << "   [dbg] after typing, child text=\"" << (c ? c->text() : QString())
            << "\"\n";
        out.flush();
    }
    sendKey(Qt::Key_Return);
    check(QStringLiteral("Tab adds + names a child"),
          doc->root()->children().size() == 1 &&
              doc->root()->children().front()->text() == QStringLiteral("Marketing"),
          QStringLiteral("children=%1").arg(doc->root()->children().size()));

    // 3. Enter chains siblings (focus stayed on the just-committed node).
    sendKey(Qt::Key_Return);
    typeText(QStringLiteral("Engineering"));
    sendKey(Qt::Key_Return);
    sendKey(Qt::Key_Return);
    typeText(QStringLiteral("Sales"));
    sendKey(Qt::Key_Return);
    check(QStringLiteral("Enter chains siblings"),
          doc->root()->children().size() == 3,
          QStringLiteral("children=%1").arg(doc->root()->children().size()));

    // 4. Tab from a child adds a grandchild.
    sendKey(Qt::Key_Tab);
    typeText(QStringLiteral("Leads"));
    sendKey(Qt::Key_Return);
    Node* sales = doc->root()->children().size() >= 3
                      ? doc->root()->children().at(2).get()
                      : nullptr;
    check(QStringLiteral("Tab from child adds grandchild"),
          sales && sales->children().size() == 1 &&
              sales->children().front()->text() == QStringLiteral("Leads"),
          sales ? QStringLiteral("sales kids=%1").arg(sales->children().size())
                : QStringLiteral("no sales node"));

    // 5. Escape cancels an edit (text unchanged).
    Node* mkt = doc->root()->children().front().get();
    focusNode(mkt);
    sendKey(Qt::Key_F2);
    typeText(QStringLiteral("ZZZ"));
    sendKey(Qt::Key_Escape);
    check(QStringLiteral("Escape cancels edit"),
          mkt->text() == QStringLiteral("Marketing"), mkt->text());

    // 6. Arrow navigation: from root, Right selects a child; Down moves among them.
    focusNode(doc->root());
    sendKey(Qt::Key_Right);
    Node* afterRight = selectedNode();
    check(QStringLiteral("Right selects a child"),
          afterRight && afterRight->parent() == doc->root(), selectedText());
    sendKey(Qt::Key_Down);
    Node* afterDown = selectedNode();
    check(QStringLiteral("Down moves to another node"),
          afterDown && afterDown != afterRight, selectedText());
    sendKey(Qt::Key_Left);
    check(QStringLiteral("Left returns toward root"),
          selectedNode() == doc->root(), selectedText());

    // 7. Delete removes the selected node; Ctrl+Z restores it.
    focusNode(sales);
    const int before = static_cast<int>(doc->root()->children().size());
    sendKey(Qt::Key_Delete);
    const int afterDel = static_cast<int>(doc->root()->children().size());
    check(QStringLiteral("Delete removes selected node"), afterDel == before - 1,
          QStringLiteral("%1 -> %2").arg(before).arg(afterDel));
    sendKey(Qt::Key_Z, Qt::ControlModifier);
    check(QStringLiteral("Ctrl+Z restores it"),
          static_cast<int>(doc->root()->children().size()) == before,
          QStringLiteral("children=%1").arg(doc->root()->children().size()));

    view->zoomToFit();
    settle(app);
    w.grab().save(QStringLiteral("/tmp/uxk-final.png"));

    out << (g_failures == 0 ? "\nALL KEYBOARD CONTROLS OK\n"
                            : QStringLiteral("\n%1 FAILURE(S)\n").arg(g_failures));
    return g_failures == 0 ? 0 : 1;
}
