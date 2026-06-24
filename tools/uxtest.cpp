// UX walkthrough harness: drives the real MainWindow through realistic flows and
// grabs the full window (chrome included) at each step for review.
#include "app/MainWindow.h"
#include "canvas/MindMapView.h"
#include "model/Document.h"
#include "model/Node.h"
#include "model/NodeStyle.h"
#include "model/Theme.h"
#include "outline/OutlineView.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QString>
#include <QtTest/QtTest>

using namespace mindflow;

static void settle(QApplication& app) {
    for (int i = 0; i < 8; ++i) {
        app.processEvents();
        QTest::qWait(40);
    }
}

static void shot(QWidget* w, const QString& name) {
    w->grab().save(QStringLiteral("/tmp/ux-") + name + QStringLiteral(".png"));
}

static QAction* findAction(QWidget* w, const QString& text) {
    for (QAction* a : w->findChildren<QAction*>())
        if (a->text() == text)
            return a;
    return nullptr;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w;
    w.resize(1200, 800);
    w.show();
    settle(app);
    shot(&w, "01-startup");

    auto* view = w.findChild<MindMapView*>();
    auto* doc = w.findChild<Document*>();
    Q_ASSERT(view && doc);

    // Build a realistic map (same model ops the Tab/Enter gestures invoke).
    Node* root = doc->root();
    doc->setNodeText(root, QStringLiteral("Product Launch"));
    Node* mkt = doc->addChild(root, QStringLiteral("Marketing"));
    doc->addChild(mkt, QStringLiteral("Social"));
    doc->addChild(mkt, QStringLiteral("Email"));
    Node* eng = doc->addChild(root, QStringLiteral("Engineering"));
    Node* api = doc->addChild(eng, QStringLiteral("API"));
    doc->addChild(eng, QStringLiteral("Mobile app"));
    Node* ops = doc->addChild(root, QStringLiteral("Operations"));
    doc->addChild(ops, QStringLiteral("Support"));
    settle(app);
    view->zoomToFit();
    settle(app);
    shot(&w, "02-map-built");

    // Select a node -> inspector should populate.
    view->selectNode(api);
    settle(app);
    shot(&w, "03-selection-inspector");

    // Make tasks via model, mark one done (mirrors checkbox clicks).
    {
        NodeContent c = api->content();
        c.isTask = true;
        c.taskDone = true;
        doc->setNodeContent(api, c);
        Node* mobile = eng->children().at(1).get();
        NodeContent c2 = mobile->content();
        c2.isTask = true;
        doc->setNodeContent(mobile, c2);
        NodeContent c3 = mkt->content();
        c3.tags = {QStringLiteral("priority")};
        c3.sticker = QStringLiteral("📣");
        doc->setNodeContent(mkt, c3);
    }
    settle(app);
    view->zoomToFit();
    settle(app);
    shot(&w, "04-tasks-tags-sticker");

    // Change the selected node's shape through the Inspector's shape combo.
    view->selectNode(ops);
    settle(app);
    const auto combos = w.findChildren<QComboBox*>();
    if (!combos.isEmpty()) {
        combos.first()->setCurrentIndex(3); // Cloud (per inspector shape order)
    }
    settle(app);
    shot(&w, "05-shape-cloud");

    // Cross-connection between two branches (mirrors Ctrl+L).
    doc->addConnection(mkt, ops);
    settle(app);
    shot(&w, "06-connection");

    // Apply the Midnight theme.
    doc->applyTheme(Theme::byName(QStringLiteral("Midnight")));
    view->setBackgroundBrush(doc->theme().canvas);
    settle(app);
    view->zoomToFit();
    settle(app);
    shot(&w, "07-theme-midnight");

    // Focus on a branch.
    view->selectNode(eng);
    settle(app);
    view->toggleFocusOnSelection();
    settle(app);
    shot(&w, "08-focus");
    view->clearFocus();
    settle(app);
    shot(&w, "08b-focus-cleared");

    // Switch to the outline view via the real menu action (checkable -> one trigger).
    if (QAction* outline = findAction(&w, QStringLiteral("&Outline")))
        outline->trigger();
    settle(app);
    shot(&w, "09-outline");

    return 0;
}
