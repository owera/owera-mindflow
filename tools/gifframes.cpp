// Dumps stop-motion frames of a mind map being built branch-by-branch (as it grows
// under keyboard entry), at constant framing, for assembly into an animated GIF
// with ffmpeg. Frames go to argv[1] (a directory) as f000.png, f001.png, …
#include "canvas/MindMapView.h"
#include "model/Document.h"
#include "model/Node.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFrame>

#include <functional>
#include <vector>

using namespace mindflow;

// The build script: each lambda performs one entry step on the document.
static std::vector<std::function<void(Document&)>> buildScript() {
    return {
        [](Document& d) { d.setNodeText(d.root(), QStringLiteral("Weekend in Lisbon")); },
        [](Document& d) { d.addChild(d.root(), QStringLiteral("Stay")); },
        [](Document& d) { d.addChild(d.root()->children().at(0).get(), QStringLiteral("Alfama flat")); },
        [](Document& d) { d.addChild(d.root()->children().at(0).get(), QStringLiteral("Check-in 3pm")); },
        [](Document& d) { d.addChild(d.root(), QStringLiteral("Food")); },
        [](Document& d) { d.addChild(d.root()->children().at(1).get(), QStringLiteral("Time Out Market")); },
        [](Document& d) { d.addChild(d.root()->children().at(1).get(), QStringLiteral("Pastéis de Belém")); },
        [](Document& d) { d.addChild(d.root(), QStringLiteral("Sights")); },
        [](Document& d) { d.addChild(d.root()->children().at(2).get(), QStringLiteral("Belém Tower")); },
        [](Document& d) { d.addChild(d.root()->children().at(2).get(), QStringLiteral("Tram 28")); },
        [](Document& d) { d.addChild(d.root(), QStringLiteral("Getting around")); },
        [](Document& d) { d.addChild(d.root()->children().at(3).get(), QStringLiteral("Metro day pass")); },
    };
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    const QString dir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                 : QStringLiteral("/tmp/mfgif");
    QDir().mkpath(dir);

    const auto script = buildScript();

    // Pass 1: build the whole map to learn the final framing rectangle.
    Document probe;
    for (const auto& step : script)
        step(probe);
    MindMapView measure;
    measure.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    measure.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    measure.setFrameShape(QFrame::NoFrame);
    measure.setDocument(&probe);
    measure.resize(960, 560);
    measure.show();
    for (int i = 0; i < 8; ++i)
        app.processEvents();
    const QRectF full = measure.scene()->itemsBoundingRect().adjusted(-50, -50, 50, 50);
    measure.hide();

    // Pass 2: rebuild step-by-step, grabbing a frame after each, with constant framing.
    Document doc;
    MindMapView view;
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setFrameShape(QFrame::NoFrame);
    view.setBackgroundBrush(doc.theme().canvas);
    view.setDocument(&doc);
    view.resize(960, 560);
    view.show();
    for (int i = 0; i < 8; ++i)
        app.processEvents();

    // Pump the event loop for real wall-clock time so the 220ms layout animation
    // fully settles before each grab (plain processEvents() advances no real time).
    auto wait = [&](int ms) {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < ms)
            app.processEvents();
    };

    int frame = 0;
    auto grab = [&] {
        wait(320); // let the layout animation finish
        view.fitInView(full, Qt::KeepAspectRatio);
        wait(40);
        view.grab().save(QStringLiteral("%1/f%2.png").arg(dir).arg(frame++, 3, 10, QLatin1Char('0')));
    };

    for (const auto& step : script) {
        step(doc);
        grab();
    }
    // Hold on the finished map for a moment (extra frames).
    for (int i = 0; i < 4; ++i)
        grab();

    return 0;
}
