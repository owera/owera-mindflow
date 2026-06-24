// Offscreen render harness: builds a sample map, lays it out, and grabs the
// MindMapView to a PNG. Used for visual verification of the canvas rendering.
#include "canvas/MindMapView.h"
#include "io/Exporters.h"
#include "model/Document.h"
#include "model/Node.h"
#include "outline/OutlineView.h"

#include <QApplication>
#include <QFileInfo>
#include <QFrame>
#include <QPixmap>
#include <QTextStream>

using namespace mindflow;

// Hide scrollbars for clean screenshots (the huge scene rect would otherwise show
// them at the viewport edges).
static void clean(MindMapView& v) {
    v.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v.setFrameShape(QFrame::NoFrame);
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    Document doc;
    Node* root = doc.root();
    doc.setNodeText(root, QStringLiteral("Weekend in Lisbon"));
    // Optional 2nd arg sets the root layout direction: organic|right|left|down|up|compact
    if (argc > 2)
        root->layoutDirection = layoutDirectionFromString(QString::fromLocal8Bit(argv[2]));
    Node* a = doc.addChild(root, QStringLiteral("Stay"));
    doc.addChild(a, QStringLiteral("Alfama flat"));
    doc.addChild(a, QStringLiteral("Check-in 3pm"));
    Node* b = doc.addChild(root, QStringLiteral("Food"));
    doc.addChild(b, QStringLiteral("Time Out Market"));
    doc.addChild(b, QStringLiteral("Pastéis de Belém"));
    Node* c = doc.addChild(root, QStringLiteral("Sights"));
    doc.addChild(c, QStringLiteral("Belém Tower"));
    doc.addChild(c, QStringLiteral("Tram 28"));
    Node* d = doc.addChild(root, QStringLiteral("Getting around"));
    doc.addChild(d, QStringLiteral("Metro day pass"));

    const QString mode = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();

    // M7 exercise: export the sample to every format and report results.
    if (mode == QLatin1String("export")) {
        MindMapView v;
        v.setDocument(&doc);
        clean(v);
        v.resize(900, 560);
        v.show();
        app.processEvents();
        v.zoomToFit();
        app.processEvents();
        QTextStream cout(stdout);
        using namespace mindflow::exporters;
        cout << "PNG  " << (toPng(v.scene(), "/tmp/mf_e.png", Qt::white) ? "ok" : "FAIL")
             << "\n";
        cout << "SVG  " << (toSvg(v.scene(), "/tmp/mf_e.svg") ? "ok" : "FAIL") << "\n";
        cout << "PDF  " << (toPdf(v.scene(), "/tmp/mf_e.pdf") ? "ok" : "FAIL") << "\n";
        writeTextFile("/tmp/mf_e.md", toMarkdown(doc));
        writeTextFile("/tmp/mf_e.opml", toOpml(doc));
        writeTextFile("/tmp/mf_e.mm", toFreeMind(doc));
        writeTextFile("/tmp/mf_e.csv", toCsv(doc));
        cout << "--- markdown ---\n" << toMarkdown(doc);
        return 0;
    }

    // M6 exercise: the synced Outline view with a task and a sticker.
    if (mode == QLatin1String("outline")) {
        {
            NodeContent ca = a->content(); // Stay
            ca.sticker = QStringLiteral("🏠");
            doc.setNodeContent(a, ca);
        }
        {
            Node* flat = a->children().front().get(); // Alfama flat
            NodeContent c = flat->content();
            c.isTask = true;
            c.taskDone = true;
            doc.setNodeContent(flat, c);
        }
        OutlineView ov;
        ov.setDocument(&doc);
        ov.resize(420, 360);
        ov.show();
        app.processEvents();
        const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                     : QStringLiteral("/tmp/mindflow_sample.png");
        return ov.grab().save(out) ? 0 : 1;
    }

    // Shape gallery: one child per shape, each labelled and styled.
    if (mode == QLatin1String("shapes")) {
        Document gal;
        Node* groot = gal.root();
        gal.setNodeText(groot, QStringLiteral("Shapes"));
        groot->layoutDirection = LayoutDirection::Down;
        const std::pair<const char*, NodeShape> shapes[] = {
            {"Rectangle", NodeShape::Rectangle}, {"Rounded", NodeShape::Rounded},
            {"Pill", NodeShape::Pill},           {"Cloud", NodeShape::Cloud},
            {"Hexagon", NodeShape::Hexagon},     {"Octagon", NodeShape::Octagon},
            {"Line", NodeShape::Line},           {"Embedded", NodeShape::Embedded},
        };
        for (const auto& s : shapes) {
            Node* n = gal.addChild(groot, QString::fromLatin1(s.first));
            NodeStyle st = n->style;
            st.shape = s.second;
            gal.setNodeStyle(n, st);
        }
        MindMapView gv;
        gv.setDocument(&gal);
        clean(gv);
        gv.resize(1000, 320);
        gv.show();
        app.processEvents();
        gv.zoomToFit();
        app.processEvents();
        const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                     : QStringLiteral("/tmp/mindflow_sample.png");
        return gv.grab().save(out) ? 0 : 1;
    }

    // M4 exercises: stickers, tasks + progress, tags, notes.
    if (mode == QLatin1String("content")) {
        Document d;
        Node* r = d.root();
        d.setNodeText(r, QStringLiteral("Trip Plan"));
        Node* pack = d.addChild(r, QStringLiteral("Packing"));
        Node* t1 = d.addChild(pack, QStringLiteral("Passport"));
        Node* t2 = d.addChild(pack, QStringLiteral("Charger"));
        Node* book = d.addChild(r, QStringLiteral("Bookings"));
        Node* note = d.addChild(book, QStringLiteral("Hotel"));
        {
            NodeContent c = pack->content();
            c.sticker = QStringLiteral("🎒");
            d.setNodeContent(pack, c);
        }
        {
            NodeContent c = t1->content();
            c.isTask = true;
            c.taskDone = true;
            d.setNodeContent(t1, c);
        }
        {
            NodeContent c = t2->content();
            c.isTask = true;
            d.setNodeContent(t2, c);
        }
        {
            NodeContent c = book->content();
            c.tags = {QStringLiteral("urgent"), QStringLiteral("$$$")};
            d.setNodeContent(book, c);
        }
        {
            NodeContent c = note->content();
            c.note = QStringLiteral("Check-in after 3pm");
            d.setNodeContent(note, c);
        }
        MindMapView v;
        v.setDocument(&d);
        clean(v);
        v.setBackgroundBrush(d.theme().canvas);
        v.resize(900, 480);
        v.show();
        app.processEvents();
        if (argc > 4)
            v.setTagHighlight(QString::fromLocal8Bit(argv[4])); // e.g. "urgent"
        v.zoomToFit();
        app.processEvents();
        const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                     : QStringLiteral("/tmp/mindflow_sample.png");
        return v.grab().save(out) ? 0 : 1;
    }

    // M5 exercises: a labelled, curved cross-connection between two branches.
    if (mode == QLatin1String("connect")) {
        Node* belem = c->children().front().get();     // Belém Tower (Sights, right)
        Node* metro = d->children().front().get();      // Metro day pass (left)
        doc.addConnection(belem, metro);
        const int cid = doc.connections().front().id;
        doc.setConnectionLabel(cid, QStringLiteral("take the metro"));
        doc.setConnectionWaypoint(cid, QPointF(0, 150)); // bow the curve down so it reads
        MindMapView v;
        v.setDocument(&doc);
        clean(v);
        v.setBackgroundBrush(doc.theme().canvas);
        v.resize(900, 560);
        v.show();
        app.processEvents();
        v.zoomToFit();
        app.processEvents();
        const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                     : QStringLiteral("/tmp/mindflow_sample.png");
        return v.grab().save(out) ? 0 : 1;
    }

    // M2 exercises: collapse one branch, detach another into a second root.
    if (mode == QLatin1String("m2")) {
        doc.setCollapsed(b, true);                 // Model branch folded
        doc.detachAsRoot(c, QPointF(360, 220));    // Layout becomes its own tree
    }

    if (mode == QLatin1String("dark"))
        doc.applyTheme(Theme::byName(QStringLiteral("Midnight")));

    MindMapView view;
    view.setDocument(&doc);
    clean(view);
    view.setBackgroundBrush(doc.theme().canvas);
    view.resize(900, 600);
    view.show();
    app.processEvents();
    if (mode == QLatin1String("focus")) {
        view.searchAndSelect(QStringLiteral("Sights")); // select the Sights branch
        view.toggleFocusOnSelection();
    }
    view.zoomToFit();
    app.processEvents();

    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                 : QStringLiteral("/tmp/mindflow_sample.png");
    QPixmap pm = view.grab();
    return pm.save(out) ? 0 : 1;
}
