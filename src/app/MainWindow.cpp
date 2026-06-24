#include "app/MainWindow.h"

#include "app/Inspector.h"
#include "canvas/MindMapView.h"
#include "io/DocumentStore.h"
#include "io/Exporters.h"
#include "io/Importers.h"
#include "model/Document.h"
#include "model/Node.h"
#include "model/Theme.h"
#include "outline/OutlineView.h"

#include <functional>

#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QUndoStack>

namespace mindflow {

namespace {
constexpr auto kFileFilter = "Owera MindFlow Map (*.mindflow);;All Files (*)";
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_doc = new Document(this);
    m_view = new MindMapView(this);
    m_view->setDocument(m_doc);
    m_outline = new OutlineView(this);
    m_outline->setDocument(m_doc);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_view);     // index 0 = map
    m_stack->addWidget(m_outline);  // index 1 = outline
    setCentralWidget(m_stack);

    m_inspector = new Inspector(this);
    m_inspector->setDocument(m_doc);
    auto* dock = new QDockWidget(tr("Style"), this);
    dock->setWidget(m_inspector);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    buildActions();

    connect(m_doc, &Document::modified, this, &MainWindow::updateTitle);
    connect(m_doc, &Document::themeChanged, this, &MainWindow::applyThemeToView);
    // Both views drive the inspector and keep each other's selection in sync.
    connect(m_view, &MindMapView::selectionChanged, this, &MainWindow::syncFromView);
    connect(m_outline, &OutlineView::selectionChanged, this, &MainWindow::syncFromOutline);

    // Accessibility: name the major regions for screen readers.
    m_view->setAccessibleName(tr("Mind map canvas"));
    m_outline->setAccessibleName(tr("Outline"));
    m_inspector->setAccessibleName(tr("Style inspector"));
    statusBar()->showMessage(
        tr("Tab: add child   Enter: add sibling   F2: rename   Ctrl+L: connect"));

    applyThemeToView();
    resize(1180, 780);
    readSettings();
    updateTitle();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    writeSettings();
    event->accept();
}

void MainWindow::readSettings() {
    QSettings settings;
    const QByteArray geom = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    const QString themeName =
        settings.value(QStringLiteral("theme"), QStringLiteral("Default")).toString();
    if (themeName != m_doc->theme().name) {
        m_doc->applyTheme(Theme::byName(themeName));
        applyThemeToView();
    }
    // A freshly opened window starts clean regardless of the startup theme apply.
    m_doc->undoStack()->clear();
    m_doc->undoStack()->setClean();
}

void MainWindow::writeSettings() {
    QSettings settings;
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("theme"), m_doc->theme().name);
}

void MainWindow::showShortcuts() {
    QMessageBox::information(
        this, tr("Keyboard Shortcuts"),
        tr("<b>Editing</b><br>"
           "Tab — add child<br>Enter — add sibling<br>F2 — rename<br>"
           "Delete — delete node<br>Ctrl+L — connect selected<br>"
           "Ctrl+. — toggle fold<br>Ctrl+Shift+D — detach as new tree<br><br>"
           "<b>View</b><br>"
           "Ctrl+E — outline view<br>Ctrl+Shift+F — focus on selection<br>"
           "Esc — exit focus<br>Ctrl+F — find<br>"
           "Ctrl++ / Ctrl+- — zoom<br>Ctrl+9 — zoom to fit<br><br>"
           "<b>File</b><br>Ctrl+N/O/S — new / open / save"));
}

MainWindow::~MainWindow() {
    // Detach view/outline/document signals before the base class tears down child
    // widgets — otherwise a late selectionChanged from the dying scene would be
    // delivered to this already-destroyed MainWindow (assert / crash on exit).
    if (m_view)
        m_view->disconnect(this);
    if (m_outline)
        m_outline->disconnect(this);
    if (m_doc)
        m_doc->disconnect(this);
}

// Selection sync: each view's change updates the inspector and mirrors to the OTHER
// view only — never back to the originating view. Re-syncing the map to itself would
// collapse a multi-node selection down to one, which would make "connect the two
// selected nodes" (Ctrl+L) impossible.
void MainWindow::syncFromView(Node* node) {
    m_inspector->setSelectedNode(node);
    if (m_syncingSelection)
        return;
    m_syncingSelection = true;
    m_outline->selectNode(node);
    m_syncingSelection = false;
}

void MainWindow::syncFromOutline(Node* node) {
    m_inspector->setSelectedNode(node);
    if (m_syncingSelection)
        return;
    m_syncingSelection = true;
    m_view->selectNode(node);
    m_syncingSelection = false;
}

void MainWindow::applyThemeToView() {
    m_view->setBackgroundBrush(m_doc->theme().canvas);
}

void MainWindow::buildImportExportMenus(QMenu* fileMenu) {
    QMenu* exportMenu = fileMenu->addMenu(tr("&Export As"));

    auto exportText = [this](const QString& filter, const QString& ext,
                             std::function<QString()> producer) {
        QString path = QFileDialog::getSaveFileName(this, tr("Export"), QString(), filter);
        if (path.isEmpty())
            return;
        if (!path.endsWith(ext))
            path += ext;
        if (!exporters::writeTextFile(path, producer()))
            QMessageBox::critical(this, tr("Export Failed"), tr("Could not write file."));
    };

    exportMenu->addAction(tr("PNG Image…"), this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, tr("Export PNG"),
                                                          QString(), tr("PNG (*.png)"));
        if (!path.isEmpty() &&
            !exporters::toPng(m_view->scene(), path, m_doc->theme().canvas))
            QMessageBox::critical(this, tr("Export Failed"), tr("Could not write image."));
    });
    exportMenu->addAction(tr("SVG Vector…"), this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, tr("Export SVG"),
                                                          QString(), tr("SVG (*.svg)"));
        if (!path.isEmpty() && !exporters::toSvg(m_view->scene(), path))
            QMessageBox::critical(this, tr("Export Failed"), tr("Could not write SVG."));
    });
    exportMenu->addAction(tr("PDF Document…"), this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, tr("Export PDF"),
                                                          QString(), tr("PDF (*.pdf)"));
        if (!path.isEmpty() && !exporters::toPdf(m_view->scene(), path))
            QMessageBox::critical(this, tr("Export Failed"), tr("Could not write PDF."));
    });
    exportMenu->addSeparator();
    exportMenu->addAction(tr("Markdown…"), this, [this, exportText] {
        exportText(tr("Markdown (*.md)"), QStringLiteral(".md"),
                   [this] { return exporters::toMarkdown(*m_doc); });
    });
    exportMenu->addAction(tr("OPML…"), this, [this, exportText] {
        exportText(tr("OPML (*.opml)"), QStringLiteral(".opml"),
                   [this] { return exporters::toOpml(*m_doc); });
    });
    exportMenu->addAction(tr("FreeMind…"), this, [this, exportText] {
        exportText(tr("FreeMind (*.mm)"), QStringLiteral(".mm"),
                   [this] { return exporters::toFreeMind(*m_doc); });
    });
    exportMenu->addAction(tr("CSV…"), this, [this, exportText] {
        exportText(tr("CSV (*.csv)"), QStringLiteral(".csv"),
                   [this] { return exporters::toCsv(*m_doc); });
    });
    exportMenu->addAction(tr("Plain Text…"), this, [this, exportText] {
        exportText(tr("Text (*.txt)"), QStringLiteral(".txt"),
                   [this] { return exporters::toPlainText(*m_doc); });
    });

    QMenu* importMenu = fileMenu->addMenu(tr("&Import"));
    using Parser = std::function<std::vector<std::unique_ptr<Node>>(const QString&)>;
    auto importWith = [this](const QString& filter, Parser parser) {
        if (!maybeSave())
            return;
        const QString path =
            QFileDialog::getOpenFileName(this, tr("Import"), QString(), filter);
        if (path.isEmpty())
            return;
        bool ok = false;
        const QString text = importers::readTextFile(path, &ok);
        if (!ok) {
            QMessageBox::critical(this, tr("Import Failed"), tr("Could not read file."));
            return;
        }
        auto roots = parser(text);
        if (roots.empty()) {
            QMessageBox::warning(this, tr("Import"), tr("Nothing to import."));
            return;
        }
        m_doc->setImportedRoots(std::move(roots), QFileInfo(path).completeBaseName());
        m_currentPath.clear();
        applyThemeToView();
        updateTitle();
    };

    importMenu->addAction(tr("OPML…"), this, [importWith] {
        importWith(tr("OPML (*.opml *.xml)"), importers::fromOpml);
    });
    importMenu->addAction(tr("Markdown…"), this, [importWith] {
        importWith(tr("Markdown (*.md *.markdown *.txt)"), importers::fromMarkdown);
    });
    importMenu->addAction(tr("FreeMind…"), this, [importWith] {
        importWith(tr("FreeMind (*.mm)"), importers::fromFreeMind);
    });
    importMenu->addAction(tr("Plain Text…"), this, [importWith] {
        importWith(tr("Text (*.txt)"), importers::fromPlainText);
    });
}

void MainWindow::buildActions() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QToolBar* tb = addToolBar(tr("Main"));
    tb->setMovable(false);

    auto* newAct = fileMenu->addAction(tr("&New"), QKeySequence::New, this,
                                       &MainWindow::newDocument);
    auto* openAct = fileMenu->addAction(tr("&Open…"), QKeySequence::Open, this,
                                        &MainWindow::openDocument);
    auto* saveAct = fileMenu->addAction(tr("&Save"), QKeySequence::Save, this,
                                        [this] { saveDocument(); });
    fileMenu->addAction(tr("Save &As…"), QKeySequence::SaveAs, this,
                        &MainWindow::saveDocumentAs);
    fileMenu->addSeparator();
    buildImportExportMenus(fileMenu);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);
    tb->addAction(newAct);
    tb->addAction(openAct);
    tb->addAction(saveAct);
    tb->addSeparator();

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QUndoStack* undo = m_doc->undoStack();
    QAction* undoAct = undo->createUndoAction(this, tr("&Undo"));
    undoAct->setShortcut(QKeySequence::Undo);
    QAction* redoAct = undo->createRedoAction(this, tr("&Redo"));
    redoAct->setShortcut(QKeySequence::Redo);
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    // Add Child / Rename / Delete have no menu accelerators: the canvas owns Tab,
    // F2 and Delete directly (see MindMapView::keyPressEvent/event), which avoids a
    // window-shortcut firing the same action a second time. The keys are advertised
    // in the status bar and the action text.
    auto* addChildAct = editMenu->addAction(tr("Add &Child (Tab)"), m_view,
                                            &MindMapView::addChildToSelection);
    auto* editAct = editMenu->addAction(tr("&Rename (F2)"), m_view,
                                        &MindMapView::editSelection);
    auto* delAct = editMenu->addAction(tr("&Delete (Del)"), m_view,
                                       &MindMapView::deleteSelection);
    editMenu->addSeparator();
    editMenu->addAction(tr("Toggle &Fold"), QKeySequence(Qt::CTRL | Qt::Key_Period),
                        m_view, &MindMapView::toggleCollapseSelection);
    editMenu->addAction(tr("Detac&h as New Tree"),
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), m_view,
                        &MindMapView::detachSelection);
    editMenu->addAction(tr("&Connect Selected"), QKeySequence(Qt::CTRL | Qt::Key_L),
                        m_view, &MindMapView::connectSelection);
    tb->addAction(undoAct);
    tb->addAction(redoAct);
    tb->addSeparator();
    tb->addAction(addChildAct);
    tb->addAction(editAct);
    tb->addAction(delAct);

    QMenu* layoutMenu = menuBar()->addMenu(tr("&Layout"));
    const struct {
        const char* label;
        LayoutDirection dir;
    } layouts[] = {
        {"&Organic", LayoutDirection::Organic},
        {"&Right", LayoutDirection::RightDown},
        {"&Left", LayoutDirection::LeftDown},
        {"&Down", LayoutDirection::Down},
        {"&Up", LayoutDirection::Up},
        {"&Compact", LayoutDirection::Compact},
    };
    for (const auto& l : layouts) {
        const LayoutDirection dir = l.dir;
        layoutMenu->addAction(tr(l.label), this,
                              [this, dir] { m_view->setSelectionLayoutDirection(dir); });
    }

    QMenu* themeMenu = menuBar()->addMenu(tr("&Theme"));
    for (const Theme& t : Theme::builtIns()) {
        const QString name = t.name;
        themeMenu->addAction(name, this,
                             [this, name] { m_doc->applyTheme(Theme::byName(name)); });
    }

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Keyboard Shortcuts"), this, &MainWindow::showShortcuts);
    helpMenu->addAction(tr("&About Owera MindFlow"), this, [this] {
        QMessageBox::about(
            this, tr("About Owera MindFlow"),
            tr("<b>Owera MindFlow %1</b><br>"
               "A MindNode-inspired open source mind map app for Linux.<br><br>"
               "Built with Qt 6.")
                .arg(QApplication::applicationVersion()));
    });

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    auto* outlineAct = viewMenu->addAction(tr("&Outline"));
    outlineAct->setCheckable(true);
    outlineAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(outlineAct, &QAction::toggled, this, [this](bool on) {
        m_stack->setCurrentIndex(on ? 1 : 0);
    });
    tb->addAction(outlineAct);
    tb->addSeparator();
    viewMenu->addSeparator();
    viewMenu->addAction(tr("Zoom &In"), QKeySequence::ZoomIn, m_view,
                        &MindMapView::zoomIn);
    viewMenu->addAction(tr("Zoom &Out"), QKeySequence::ZoomOut, m_view,
                        &MindMapView::zoomOut);
    viewMenu->addAction(tr("&Actual Size"), QKeySequence(Qt::CTRL | Qt::Key_0), m_view,
                        &MindMapView::resetZoom);
    viewMenu->addAction(tr("Zoom to &Fit"), QKeySequence(Qt::CTRL | Qt::Key_9), m_view,
                        &MindMapView::zoomToFit);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("&Highlight Tag…"), this, [this] {
        const QStringList tags = m_doc->allTags();
        if (tags.isEmpty()) {
            QMessageBox::information(this, tr("Tag Highlight"),
                                     tr("No tags in this document yet."));
            return;
        }
        bool ok = false;
        const QString tag = QInputDialog::getItem(this, tr("Highlight Tag"),
                                                  tr("Tag:"), tags, 0, false, &ok);
        if (ok)
            m_view->setTagHighlight(tag);
    });
    viewMenu->addAction(tr("&Clear Highlight"), this,
                        [this] { m_view->setTagHighlight(QString()); });
    viewMenu->addSeparator();
    viewMenu->addAction(tr("&Focus on Selection"),
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), m_view,
                        &MindMapView::toggleFocusOnSelection);
    viewMenu->addAction(tr("E&xit Focus"), QKeySequence(Qt::Key_Escape), m_view,
                        &MindMapView::clearFocus);
    auto* showConns = viewMenu->addAction(tr("Show &Connections"));
    showConns->setCheckable(true);
    showConns->setChecked(true);
    connect(showConns, &QAction::toggled, m_view, &MindMapView::setConnectionsVisible);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("&Find…"), QKeySequence::Find, this, [this] {
        bool ok = false;
        const QString q = QInputDialog::getText(this, tr("Find"), tr("Search text:"),
                                                QLineEdit::Normal, QString(), &ok);
        if (ok)
            m_view->searchAndSelect(q);
    });
}

void MainWindow::updateTitle() {
    const QString name = m_currentPath.isEmpty()
                             ? tr("Untitled")
                             : QFileInfo(m_currentPath).completeBaseName();
    const bool dirty = !m_doc->undoStack()->isClean();
    setWindowTitle(QStringLiteral("%1%2 — Owera MindFlow").arg(name, dirty ? "*" : ""));
}

bool MainWindow::maybeSave() {
    if (m_doc->undoStack()->isClean())
        return true;
    const auto ret = QMessageBox::warning(
        this, tr("Owera MindFlow"), tr("The document has unsaved changes. Save them?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save)
        return saveDocument();
    return ret != QMessageBox::Cancel;
}

void MainWindow::newDocument() {
    if (!maybeSave())
        return;
    m_doc->reset();
    m_currentPath.clear();
    m_view->setDocument(m_doc);
    applyThemeToView();
    m_doc->undoStack()->setClean();
    updateTitle();
}

void MainWindow::openDocument() {
    if (!maybeSave())
        return;
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Open Map"), QString(), tr(kFileFilter));
    if (path.isEmpty())
        return;
    QString error;
    if (!DocumentStore::load(*m_doc, path, &error)) {
        QMessageBox::critical(this, tr("Open Failed"), error);
        return;
    }
    m_currentPath = path;
    m_view->setDocument(m_doc);
    applyThemeToView();
    m_doc->undoStack()->setClean();
    updateTitle();
}

bool MainWindow::saveDocument() {
    if (m_currentPath.isEmpty())
        return saveDocumentAs();
    QString error;
    if (!DocumentStore::save(*m_doc, m_currentPath, &error)) {
        QMessageBox::critical(this, tr("Save Failed"), error);
        return false;
    }
    m_doc->undoStack()->setClean();
    updateTitle();
    return true;
}

bool MainWindow::saveDocumentAs() {
    QString path =
        QFileDialog::getSaveFileName(this, tr("Save Map"), QString(), tr(kFileFilter));
    if (path.isEmpty())
        return false;
    if (!path.endsWith(QStringLiteral(".mindflow")))
        path += QStringLiteral(".mindflow");
    m_currentPath = path;
    return saveDocument();
}

} // namespace mindflow
