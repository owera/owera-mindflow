#pragma once

#include <QWidget>

class QComboBox;
class QPushButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

namespace mindflow {

class Document;
class Node;

// Sidebar that edits the selected node's visual style (shape, colors, font,
// connector). Reads the selection's NodeStyle into its controls, and on any edit
// builds a new NodeStyle and pushes it through Document (undoable). An "apply to
// branch" toggle propagates shape/connector/font down the subtree.
class Inspector : public QWidget {
    Q_OBJECT
public:
    explicit Inspector(QWidget* parent = nullptr);

    void setDocument(Document* doc);

public slots:
    void setSelectedNode(mindflow::Node* node);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void commit();              // build style from controls -> Document
    void commitContent();       // build content (note/tags/task/sticker) -> Document
    void setColorButton(QPushButton* button, const QColor& color);
    void pickColor(QPushButton* button);
    void pickFont();
    void pickSticker();
    void pickImage();
    void clearImage();
    void setControlsEnabled(bool on);

    Document* m_doc = nullptr;
    Node* m_node = nullptr;
    bool m_updating = false;     // guard so populating controls doesn't commit

    QComboBox* m_shape = nullptr;
    QComboBox* m_connector = nullptr;
    QPushButton* m_fill = nullptr;
    QPushButton* m_text = nullptr;
    QPushButton* m_border = nullptr;
    QPushButton* m_font = nullptr;
    QCheckBox* m_applyBranch = nullptr;
    QLabel* m_hint = nullptr;

    // Content controls.
    QCheckBox* m_task = nullptr;
    QPushButton* m_sticker = nullptr;
    QPushButton* m_image = nullptr;
    QPushButton* m_imageClear = nullptr;
    QLineEdit* m_tags = nullptr;
    QPlainTextEdit* m_note = nullptr;
};

} // namespace mindflow
