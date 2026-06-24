#pragma once

#include <QMainWindow>
#include <QString>

class QStackedWidget;

namespace mindflow {

class Document;
class Node;
class MindMapView;
class Inspector;
class OutlineView;

// Top-level window: hosts the MindMapView, menus, toolbar, and file actions.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newDocument();
    void openDocument();
    bool saveDocument();
    bool saveDocumentAs();

private:
    void buildActions();
    void buildImportExportMenus(class QMenu* fileMenu);
    void applyThemeToView();
    void updateTitle();
    bool maybeSave();
    void readSettings();
    void writeSettings();
    void showShortcuts();

    void syncFromView(Node* node);    // map selection -> inspector + outline
    void syncFromOutline(Node* node); // outline selection -> inspector + map

    Document* m_doc = nullptr;
    MindMapView* m_view = nullptr;
    OutlineView* m_outline = nullptr;
    QStackedWidget* m_stack = nullptr;
    Inspector* m_inspector = nullptr;
    QString m_currentPath;
    bool m_syncingSelection = false;
};

} // namespace mindflow
