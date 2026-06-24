#include "app/Inspector.h"

#include "model/Document.h"
#include "model/Node.h"
#include "model/NodeStyle.h"

#include <QBuffer>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QFormLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace mindflow {

namespace {
const std::pair<const char*, NodeShape> kShapes[] = {
    {"Rounded", NodeShape::Rounded},     {"Rectangle", NodeShape::Rectangle},
    {"Pill", NodeShape::Pill},           {"Cloud", NodeShape::Cloud},
    {"Hexagon", NodeShape::Hexagon},     {"Octagon", NodeShape::Octagon},
    {"Line", NodeShape::Line},           {"Embedded", NodeShape::Embedded},
};
} // namespace

Inspector::Inspector(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    outer->addLayout(form);

    m_shape = new QComboBox(this);
    for (const auto& s : kShapes)
        m_shape->addItem(QString::fromLatin1(s.first));
    form->addRow(tr("Shape"), m_shape);

    m_connector = new QComboBox(this);
    m_connector->addItem(tr("Rounded"));
    m_connector->addItem(tr("Orthogonal"));
    form->addRow(tr("Connector"), m_connector);

    m_fill = new QPushButton(this);
    m_text = new QPushButton(this);
    m_border = new QPushButton(this);
    form->addRow(tr("Fill"), m_fill);
    form->addRow(tr("Text"), m_text);
    form->addRow(tr("Border"), m_border);

    m_font = new QPushButton(tr("Choose Font…"), this);
    form->addRow(tr("Font"), m_font);

    m_applyBranch = new QCheckBox(tr("Apply shape/font to whole branch"), this);
    outer->addWidget(m_applyBranch);

    // --- content section ---
    auto* contentForm = new QFormLayout();
    outer->addSpacing(6);
    outer->addWidget(new QLabel(tr("<b>Content</b>"), this));
    outer->addLayout(contentForm);

    m_task = new QCheckBox(tr("Task (show checkbox)"), this);
    contentForm->addRow(QString(), m_task);

    m_sticker = new QPushButton(tr("Add Sticker…"), this);
    contentForm->addRow(tr("Sticker"), m_sticker);

    auto* imgRow = new QWidget(this);
    auto* imgLayout = new QVBoxLayout(imgRow);
    imgLayout->setContentsMargins(0, 0, 0, 0);
    m_image = new QPushButton(tr("Set Image…"), imgRow);
    m_imageClear = new QPushButton(tr("Remove Image"), imgRow);
    imgLayout->addWidget(m_image);
    imgLayout->addWidget(m_imageClear);
    contentForm->addRow(tr("Image"), imgRow);

    m_tags = new QLineEdit(this);
    m_tags->setPlaceholderText(tr("comma, separated, tags"));
    contentForm->addRow(tr("Tags"), m_tags);

    m_note = new QPlainTextEdit(this);
    m_note->setPlaceholderText(tr("Notes…"));
    m_note->setMaximumHeight(90);
    contentForm->addRow(tr("Note"), m_note);

    connect(m_task, &QCheckBox::toggled, this, [this] { commitContent(); });
    connect(m_sticker, &QPushButton::clicked, this, [this] { pickSticker(); });
    connect(m_image, &QPushButton::clicked, this, [this] { pickImage(); });
    connect(m_imageClear, &QPushButton::clicked, this, [this] { clearImage(); });
    connect(m_tags, &QLineEdit::editingFinished, this, [this] { commitContent(); });
    // Commit notes when focus leaves the editor (avoids an undo step per keystroke).
    m_note->installEventFilter(this);

    m_hint = new QLabel(tr("Select a node to edit its style."), this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    outer->addWidget(m_hint);
    outer->addStretch(1);

    connect(m_shape, &QComboBox::currentIndexChanged, this, [this] { commit(); });
    connect(m_connector, &QComboBox::currentIndexChanged, this, [this] { commit(); });
    connect(m_fill, &QPushButton::clicked, this, [this] { pickColor(m_fill); });
    connect(m_text, &QPushButton::clicked, this, [this] { pickColor(m_text); });
    connect(m_border, &QPushButton::clicked, this, [this] { pickColor(m_border); });
    connect(m_font, &QPushButton::clicked, this, [this] { pickFont(); });

    setSelectedNode(nullptr);
}

void Inspector::setDocument(Document* doc) {
    m_doc = doc;
    if (m_doc) {
        // A theme switch recolors nodes; re-read the selected node so the swatches
        // reflect its new colors immediately.
        connect(m_doc, &Document::themeChanged, this, [this] {
            if (m_node)
                setSelectedNode(m_node);
        });
    }
}

void Inspector::setControlsEnabled(bool on) {
    for (QWidget* w : {static_cast<QWidget*>(m_shape), static_cast<QWidget*>(m_connector),
                       static_cast<QWidget*>(m_fill), static_cast<QWidget*>(m_text),
                       static_cast<QWidget*>(m_border), static_cast<QWidget*>(m_font),
                       static_cast<QWidget*>(m_applyBranch), static_cast<QWidget*>(m_task),
                       static_cast<QWidget*>(m_sticker), static_cast<QWidget*>(m_image),
                       static_cast<QWidget*>(m_imageClear), static_cast<QWidget*>(m_tags),
                       static_cast<QWidget*>(m_note)})
        w->setEnabled(on);
    m_hint->setVisible(!on);
}

bool Inspector::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_note && event->type() == QEvent::FocusOut)
        commitContent();
    return QWidget::eventFilter(obj, event);
}

void Inspector::setColorButton(QPushButton* button, const QColor& color) {
    button->setProperty("mfColor", color);
    if (color.alpha() == 0) {
        // "None": a neutral swatch with dark text, not an opaque black box.
        button->setText(tr("None"));
        button->setStyleSheet(QStringLiteral(
            "background-color: #e9e9ee; color: #555; border: 1px solid #c0c0c8;"));
        return;
    }
    button->setText(color.name());
    const QString fg = color.lightnessF() > 0.6 ? QStringLiteral("black")
                                                : QStringLiteral("white");
    button->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;").arg(color.name(), fg));
}

void Inspector::setSelectedNode(Node* node) {
    m_node = node;
    setControlsEnabled(node != nullptr);
    if (!node)
        return;

    m_updating = true;
    const NodeStyle& s = node->style;
    for (int i = 0; i < static_cast<int>(std::size(kShapes)); ++i)
        if (kShapes[i].second == s.shape) {
            m_shape->setCurrentIndex(i);
            break;
        }
    m_connector->setCurrentIndex(s.connector == ConnectorStyle::Orthogonal ? 1 : 0);
    setColorButton(m_fill, s.fillColor);
    setColorButton(m_text, s.textColor);
    setColorButton(m_border, s.borderColor);
    m_font->setText(QStringLiteral("%1 %2pt").arg(s.font.family()).arg(s.font.pointSize()));
    m_font->setProperty("mfFont", s.font);

    m_task->setChecked(node->isTask);
    m_sticker->setText(node->sticker.isEmpty() ? tr("Add Sticker…") : node->sticker);
    m_imageClear->setEnabled(!node->imagePng.isEmpty());
    m_image->setText(node->imagePng.isEmpty() ? tr("Set Image…") : tr("Replace Image…"));
    m_tags->setText(node->tags.join(QStringLiteral(", ")));
    m_note->setPlainText(node->note);
    m_updating = false;
}

void Inspector::commitContent() {
    if (m_updating || !m_doc || !m_node)
        return;
    NodeContent c = m_node->content();
    c.isTask = m_task->isChecked();
    QStringList tags;
    for (const QString& t : m_tags->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
        tags << t.trimmed();
    c.tags = tags;
    c.note = m_note->toPlainText();
    m_doc->setNodeContent(m_node, c);
}

void Inspector::pickSticker() {
    if (!m_doc || !m_node)
        return;
    // Searchable emoji palette (glyph + keywords).
    static const std::pair<const char*, const char*> kEmoji[] = {
        {"⭐", "star favorite important"}, {"✅", "check done complete task"},
        {"❗", "important alert warning"}, {"❓", "question help unknown"},
        {"🔥", "fire hot urgent trending"}, {"💡", "idea light bulb insight"},
        {"📌", "pin location note"},        {"📝", "note memo write"},
        {"🎯", "target goal aim"},          {"🚀", "rocket launch start ship"},
        {"🐞", "bug issue defect"},          {"💰", "money cost budget"},
        {"📅", "calendar date schedule"},   {"⏰", "clock time deadline"},
        {"❤️", "heart love like"},           {"👍", "thumbs up approve good"},
        {"⚠️", "warning caution"},           {"🔒", "lock secure private"},
        {"🎒", "bag pack travel"},           {"🏁", "flag finish done"},
        {"📈", "chart growth metrics"},      {"🧩", "puzzle piece component"},
        {"🌟", "sparkle special"},           {"🛠️", "tools build fix"},
    };

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Choose Sticker"));
    auto* lay = new QVBoxLayout(&dlg);
    auto* search = new QLineEdit(&dlg);
    search->setPlaceholderText(tr("Search…"));
    auto* list = new QListWidget(&dlg);
    list->setViewMode(QListView::IconMode);
    list->setSpacing(6);
    QFont big = list->font();
    big.setPointSize(20);
    list->setFont(big);
    lay->addWidget(search);
    lay->addWidget(list);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    auto* none = buttons->addButton(tr("Remove"), QDialogButtonBox::ResetRole);
    lay->addWidget(buttons);

    auto populate = [&](const QString& filter) {
        list->clear();
        for (const auto& e : kEmoji) {
            const QString kw = QString::fromUtf8(e.second);
            if (filter.isEmpty() || kw.contains(filter, Qt::CaseInsensitive)) {
                auto* it = new QListWidgetItem(QString::fromUtf8(e.first), list);
                it->setToolTip(kw);
            }
        }
    };
    populate(QString());
    connect(search, &QLineEdit::textChanged, &dlg, [&](const QString& f) { populate(f); });

    NodeContent chosen = m_node->content();
    bool apply = false;
    connect(list, &QListWidget::itemClicked, &dlg, [&](QListWidgetItem* it) {
        chosen.sticker = it->text();
        apply = true;
        dlg.accept();
    });
    connect(none, &QPushButton::clicked, &dlg, [&] {
        chosen.sticker.clear();
        apply = true;
        dlg.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() && apply)
        m_doc->setNodeContent(m_node, chosen);
}

void Inspector::pickImage() {
    if (!m_doc || !m_node)
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose Image"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty())
        return;
    QImage img(path);
    if (img.isNull())
        return;
    if (img.width() > 800)
        img = img.scaledToWidth(800, Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    NodeContent c = m_node->content();
    c.imagePng = bytes;
    m_doc->setNodeContent(m_node, c);
}

void Inspector::clearImage() {
    if (!m_doc || !m_node)
        return;
    NodeContent c = m_node->content();
    c.imagePng.clear();
    m_doc->setNodeContent(m_node, c);
}

void Inspector::pickColor(QPushButton* button) {
    if (!m_node)
        return;
    const QColor current = button->property("mfColor").value<QColor>();
    QColorDialog dlg(current.isValid() ? current : QColor(Qt::white), this);
    dlg.setOption(QColorDialog::ShowAlphaChannel, button == m_border);
    if (dlg.exec() != QDialog::Accepted)
        return;
    setColorButton(button, dlg.selectedColor());
    commit();
}

void Inspector::pickFont() {
    if (!m_node)
        return;
    bool ok = false;
    const QFont start = m_font->property("mfFont").value<QFont>();
    const QFont chosen = QFontDialog::getFont(&ok, start, this);
    if (!ok)
        return;
    m_font->setProperty("mfFont", chosen);
    m_font->setText(
        QStringLiteral("%1 %2pt").arg(chosen.family()).arg(chosen.pointSize()));
    commit();
}

void Inspector::commit() {
    if (m_updating || !m_doc || !m_node)
        return;
    NodeStyle s = m_node->style;
    s.shape = kShapes[m_shape->currentIndex()].second;
    s.connector =
        m_connector->currentIndex() == 1 ? ConnectorStyle::Orthogonal : ConnectorStyle::Rounded;
    s.fillColor = m_fill->property("mfColor").value<QColor>();
    s.textColor = m_text->property("mfColor").value<QColor>();
    s.borderColor = m_border->property("mfColor").value<QColor>();
    s.font = m_font->property("mfFont").value<QFont>();
    m_doc->setNodeStyle(m_node, s, m_applyBranch->isChecked());
}

} // namespace mindflow
