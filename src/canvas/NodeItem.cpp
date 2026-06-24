#include "canvas/NodeItem.h"

#include "model/Node.h"

#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextCursor>
#include <QtMath>

namespace mindflow {

namespace {
constexpr double kPadX = 14.0;
constexpr double kPadY = 8.0;
constexpr double kMinW = 40.0;
constexpr double kMinH = 28.0;

// Inline editor for a node's text. The rapid-entry command keys (Tab/Enter) are
// handled by MindMapView before they reach here, so this only needs to handle text
// editing, Escape (cancel) and focus-out (commit). Shift+Enter inserts a newline.
class InlineTextEditor : public QGraphicsTextItem {
    Q_OBJECT
public:
    using QGraphicsTextItem::QGraphicsTextItem;
signals:
    void committed(); // focus lost -> commit
    void cancelled(); // Escape -> discard
protected:
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape) {
            emit cancelled();
            return;
        }
        QGraphicsTextItem::keyPressEvent(e);
    }
    void focusOutEvent(QFocusEvent* e) override {
        QGraphicsTextItem::focusOutEvent(e);
        emit committed();
    }
};
} // namespace

NodeItem::NodeItem(Node* node, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_node(node), m_id(node->id()) {
    setFlags(ItemIsSelectable | ItemIsMovable | ItemIsFocusable |
             ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setZValue(10);
    refresh();
}

QSizeF NodeItem::size() const { return m_size; }

QPointF NodeItem::sceneCenter() const {
    return mapToScene(boundingRect().center());
}

QRectF NodeItem::boundingRect() const {
    // Top-left at origin; pos() places the top-left in scene space.
    QRectF r = QRectF(QPointF(0, 0), m_size).adjusted(-3, -3, 3, 3);
    const QRectF knob = foldKnobRect();
    if (!knob.isNull())
        r = r.united(knob.adjusted(-1, -1, 1, 1));
    return r;
}

void NodeItem::refresh() {
    // Decode the attached image into a capped thumbnail (cached).
    if (m_node->imagePng.isEmpty()) {
        m_thumb = QPixmap();
    } else {
        QPixmap pm;
        pm.loadFromData(m_node->imagePng, "PNG");
        if (!pm.isNull() && pm.width() > 200)
            pm = pm.scaledToWidth(200, Qt::SmoothTransformation);
        if (!pm.isNull() && pm.height() > 150)
            pm = pm.scaledToHeight(150, Qt::SmoothTransformation);
        m_thumb = pm;
    }
    relayoutContent();
}

void NodeItem::relayoutContent() {
    prepareGeometryChange();
    const NodeStyle& st = m_node->style;
    QFontMetricsF fm(st.font);

    m_taskDone = m_taskTotal = 0;
    m_node->taskProgress(m_taskDone, m_taskTotal);

    const bool hasCheckbox = m_node->isTask;
    const double cb = hasCheckbox ? 18.0 : 0.0;
    const double cbGap = hasCheckbox ? 6.0 : 0.0;

    QString header;
    if (!m_node->sticker.isEmpty())
        header = m_node->sticker + QStringLiteral("  ");
    header += m_node->text();
    if (header.isEmpty())
        header = QStringLiteral(" ");
    const QRectF tb = fm.boundingRect(QRectF(0, 0, 360, 0),
                                      Qt::AlignLeft | Qt::TextWordWrap, header);
    const double textW = std::max(20.0, tb.width());
    const double textH = std::max(fm.height(), tb.height());

    // Subtree task progress shown as a pill (skip if this is the lone task).
    double progW = 0.0;
    if (m_taskTotal > 0 && !(hasCheckbox && m_taskTotal == 1)) {
        const QString p = QStringLiteral("%1/%2").arg(m_taskDone).arg(m_taskTotal);
        progW = fm.horizontalAdvance(p) + 16.0;
    }

    const double imgW = m_thumb.isNull() ? 0.0 : m_thumb.width();
    const double imgH = m_thumb.isNull() ? 0.0 : m_thumb.height();

    double tagsW = 0.0, tagsH = 0.0;
    if (!m_node->tags.isEmpty()) {
        QFont tf = st.font;
        tf.setPointSizeF(std::max(7.0, st.font.pointSizeF() - 2.0));
        QFontMetricsF tfm(tf);
        for (const QString& tag : m_node->tags)
            tagsW += tfm.horizontalAdvance(tag) + 16.0 + 6.0;
        tagsH = 18.0;
    }

    const double headerW = cb + cbGap + textW + (progW > 0 ? progW + 6.0 : 0.0);
    const double contentW = std::max({headerW, imgW, tagsW});
    const double width = std::max(kMinW, contentW + 2 * kPadX);

    double y = kPadY;
    double height = kPadY;
    if (imgH > 0) {
        m_imageRect = QRectF(kPadX, y, imgW, imgH);
        y += imgH + 6;
        height += imgH + 6;
    } else {
        m_imageRect = QRectF();
    }

    const double headerH = std::max(textH, cb);
    m_checkboxRect = hasCheckbox ? QRectF(kPadX, y + (headerH - cb) / 2, cb, cb) : QRectF();
    m_textArea = QRectF(kPadX + cb + cbGap, y, textW + (progW > 0 ? progW + 6 : 0), headerH);
    y += headerH;
    height += headerH;

    if (tagsH > 0) {
        m_tagsRect = QRectF(kPadX, y + 4, tagsW, tagsH);
        height += 4 + tagsH;
    } else {
        m_tagsRect = QRectF();
    }

    height += kPadY;
    m_size = QSizeF(width, std::max(kMinH, height));
    update();
}

void NodeItem::syncPositionFromModel() {
    // model stores center; item pos is top-left
    setTopLeft(m_node->layoutPos - QPointF(m_size.width() / 2.0, m_size.height() / 2.0));
}

void NodeItem::setTopLeft(const QPointF& topLeft) {
    m_suppressMoveCommit = true;
    setPos(topLeft);
    m_suppressMoveCommit = false;
}

void NodeItem::setFoldHint(bool hasChildren, const QPointF& childrenSceneCenter) {
    const bool side = childrenSceneCenter.x() >= sceneCenter().x();
    const int newSide = side ? +1 : -1;
    if (hasChildren == m_hasChildren && newSide == m_foldSide)
        return;
    prepareGeometryChange();
    m_hasChildren = hasChildren;
    m_foldSide = newSide;
    update();
}

QRectF NodeItem::foldKnobRect() const {
    if (!m_hasChildren)
        return QRectF();
    const double r = 9.0;
    const double cy = m_size.height() / 2.0;
    const double cx = m_foldSide > 0 ? m_size.width() + r + 2.0 : -(r + 2.0);
    return QRectF(cx - r, cy - r, 2 * r, 2 * r);
}

QPainterPath NodeItem::shapePath(const QRectF& r) const {
    QPainterPath path;
    const double w = r.width(), h = r.height();
    switch (m_node->style.shape) {
    case NodeShape::Rectangle:
        path.addRect(r);
        break;
    case NodeShape::Pill:
        path.addRoundedRect(r, r.height() / 2.0, r.height() / 2.0);
        break;
    case NodeShape::Line:
    case NodeShape::Embedded:
        path.addRect(r); // visual differences handled in paint()
        break;
    case NodeShape::Hexagon: {
        const double inset = std::min(h / 2.0, w * 0.22);
        path.moveTo(r.left() + inset, r.top());
        path.lineTo(r.right() - inset, r.top());
        path.lineTo(r.right(), r.center().y());
        path.lineTo(r.right() - inset, r.bottom());
        path.lineTo(r.left() + inset, r.bottom());
        path.lineTo(r.left(), r.center().y());
        path.closeSubpath();
        break;
    }
    case NodeShape::Octagon: {
        const double c = std::min(w, h) * 0.28;
        path.moveTo(r.left() + c, r.top());
        path.lineTo(r.right() - c, r.top());
        path.lineTo(r.right(), r.top() + c);
        path.lineTo(r.right(), r.bottom() - c);
        path.lineTo(r.right() - c, r.bottom());
        path.lineTo(r.left() + c, r.bottom());
        path.lineTo(r.left(), r.bottom() - c);
        path.lineTo(r.left(), r.top() + c);
        path.closeSubpath();
        break;
    }
    case NodeShape::Cloud: {
        // Union of overlapping ellipses (winding fill) -> a puffy silhouette that
        // stays within r so the bounding rect needs no expansion.
        path.setFillRule(Qt::WindingFill);
        const double bx = std::min(w * 0.22, h * 0.5);
        const double by = h * 0.32;
        path.addRoundedRect(r.adjusted(bx * 0.6, by * 0.6, -bx * 0.6, -by * 0.6), 6, 6);
        const double cy = r.center().y();
        auto bump = [&](double cx, double ry, double rad) {
            path.addEllipse(QPointF(cx, ry), rad, rad);
        };
        bump(r.left() + w * 0.28, r.top() + by, by);
        bump(r.left() + w * 0.62, r.top() + by * 0.85, by * 1.15);
        bump(r.left() + w * 0.28, r.bottom() - by, by);
        bump(r.left() + w * 0.66, r.bottom() - by, by);
        bump(r.left() + bx, cy, by);
        bump(r.right() - bx, cy, by);
        break;
    }
    case NodeShape::Rounded:
    default:
        path.addRoundedRect(r, 8, 8);
        break;
    }
    return path;
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QRectF r(QPointF(0, 0), m_size);
    const NodeStyle& s = m_node->style;
    const bool selected = isSelected();

    const NodeShape shape = s.shape;
    if (shape == NodeShape::Line) {
        // text on a baseline only
        painter->setPen(QPen(s.fillColor, 2));
        painter->drawLine(r.bottomLeft(), r.bottomRight());
    } else {
        QPainterPath path = shapePath(r);
        painter->fillPath(path, shape == NodeShape::Embedded ? QColor(0, 0, 0, 30)
                                                             : s.fillColor);
        if (s.borderColor.alpha() > 0) {
            painter->setPen(QPen(s.borderColor, 1.5));
            painter->drawPath(path);
        }
    }

    if (selected) {
        QPen sel(QColor(0xff, 0xa5, 0x00), 2, Qt::SolidLine);
        painter->setPen(sel);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(r.adjusted(-2, -2, 2, 2), 9, 9);
    }

    const QColor fgColor = (shape == NodeShape::Line || shape == NodeShape::Embedded)
                               ? QColor(Qt::black)
                               : s.textColor;

    // Image thumbnail.
    if (!m_thumb.isNull())
        painter->drawPixmap(m_imageRect.topLeft(), m_thumb);

    // Task checkbox.
    if (!m_checkboxRect.isNull()) {
        painter->setPen(QPen(fgColor, 1.6));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(m_checkboxRect, 3, 3);
        if (m_node->taskDone) {
            QPen tick(fgColor, 2.0);
            tick.setCapStyle(Qt::RoundCap);
            painter->setPen(tick);
            const QRectF c = m_checkboxRect.adjusted(4, 4, -4, -4);
            painter->drawLine(c.left(), c.center().y(), c.center().x(), c.bottom());
            painter->drawLine(c.center().x(), c.bottom(), c.right(), c.top());
        }
    }

    if (!m_editor) { // hide painted text while editing
        QString header;
        if (!m_node->sticker.isEmpty())
            header = m_node->sticker + QStringLiteral("  ");
        header += m_node->text();
        painter->setPen(fgColor);
        painter->setFont(s.font);
        painter->drawText(m_textArea, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                          header);
    }

    // Task progress pill (subtree), right side of the header line.
    if (m_taskTotal > 0 && !(m_node->isTask && m_taskTotal == 1)) {
        const QString p = QStringLiteral("%1/%2").arg(m_taskDone).arg(m_taskTotal);
        QFontMetricsF fm(s.font);
        const double pw = fm.horizontalAdvance(p) + 12.0;
        QRectF pill(m_textArea.right() - pw, m_textArea.center().y() - 9, pw, 18);
        const bool complete = m_taskDone == m_taskTotal;
        painter->setPen(Qt::NoPen);
        painter->setBrush(complete ? QColor(0x2f, 0xa8, 0x4f) : QColor(0, 0, 0, 40));
        painter->drawRoundedRect(pill, 9, 9);
        painter->setPen(complete ? QColor(Qt::white) : fgColor);
        painter->drawText(pill, Qt::AlignCenter, p);
    }

    // Tag chips.
    if (!m_tagsRect.isNull()) {
        QFont tf = s.font;
        tf.setPointSizeF(std::max(7.0, s.font.pointSizeF() - 2.0));
        painter->setFont(tf);
        QFontMetricsF tfm(tf);
        double x = m_tagsRect.left();
        for (const QString& tag : m_node->tags) {
            const double cw = tfm.horizontalAdvance(tag) + 16.0;
            QRectF chip(x, m_tagsRect.top(), cw, m_tagsRect.height());
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, shape == NodeShape::Embedded ? 120 : 70));
            painter->drawRoundedRect(chip, 9, 9);
            painter->setPen(fgColor);
            painter->drawText(chip, Qt::AlignCenter, tag);
            x += cw + 6.0;
        }
        painter->setFont(s.font);
    }

    // Note indicator (top-right corner).
    if (!m_node->note.isEmpty()) {
        const QRectF dot(m_size.width() - 12, 4, 7, 7);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0xff, 0xc1, 0x07));
        painter->drawEllipse(dot);
    }

    const QRectF knob = foldKnobRect();
    if (!knob.isNull()) {
        painter->setPen(QPen(QColor(0x88, 0x90, 0x9c), 1.4));
        painter->setBrush(QColor(Qt::white));
        painter->drawEllipse(knob);
        // "+" when collapsed (can expand), "−" when expanded (can collapse)
        painter->setPen(QPen(QColor(0x55, 0x5c, 0x69), 1.6));
        const QPointF c = knob.center();
        const double a = knob.width() / 4.0;
        painter->drawLine(QPointF(c.x() - a, c.y()), QPointF(c.x() + a, c.y()));
        if (m_node->collapsed)
            painter->drawLine(QPointF(c.x(), c.y() - a), QPointF(c.x(), c.y() + a));
    }
}

void NodeItem::beginEditing() {
    if (m_editor)
        return;
    auto* editor = new InlineTextEditor(this);
    m_editor = editor;
    editor->setFont(m_node->style.font);
    editor->setDefaultTextColor(m_node->style.textColor.value() < 128 ? QColor(Qt::white)
                                                                       : QColor(Qt::black));
    editor->setPlainText(m_node->text());
    editor->setTextInteractionFlags(Qt::TextEditorInteraction);
    editor->setPos(m_textArea.topLeft() - QPointF(4, 4));
    editor->setFocus();
    QTextCursor cur = editor->textCursor();
    cur.select(QTextCursor::Document);
    editor->setTextCursor(cur);

    connect(editor, &InlineTextEditor::committed, this,
            [this] { finishEditing(true, /*refocus=*/true); });
    connect(editor, &InlineTextEditor::cancelled, this,
            [this] { finishEditing(false, /*refocus=*/true); });
    update();
}

bool NodeItem::isEditing() const { return m_editor != nullptr; }

void NodeItem::commitEditing() { finishEditing(/*commit=*/true, /*refocus=*/false); }

void NodeItem::cancelEditing() { finishEditing(/*commit=*/false, /*refocus=*/true); }

void NodeItem::finishEditing(bool commit, bool refocus) {
    if (!m_editor)
        return;
    const QString text = m_editor->toPlainText();
    auto* editor = m_editor;
    m_editor = nullptr;          // clear first so paint() draws text again
    editor->deleteLater();
    if (commit)
        emit textCommitted(m_node, text);
    if (refocus && scene())
        setFocus(); // return keyboard focus to the node for the next Tab/Enter
    update();
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    // A click on the fold knob toggles collapse instead of selecting/dragging.
    const QRectF knob = foldKnobRect();
    if (!knob.isNull() && knob.contains(event->pos())) {
        emit toggleCollapseRequested(m_node);
        event->accept();
        return;
    }
    // A click on the checkbox toggles the task's done state.
    if (!m_checkboxRect.isNull() && m_checkboxRect.contains(event->pos())) {
        emit toggleTaskRequested(m_node);
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) {
    Q_UNUSED(event);
    beginEditing();
}

void NodeItem::keyPressEvent(QKeyEvent* event) {
    // Command keys (Tab/Enter/F2/Delete/arrows) are handled by MindMapView before
    // they reach an item; here we only forward to the base (e.g. text editing keys
    // when the inline editor is active).
    QGraphicsObject::keyPressEvent(event);
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionHasChanged)
        emit geometryChanged();
    return QGraphicsObject::itemChange(change, value);
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsObject::mouseReleaseEvent(event);
    if (!m_suppressMoveCommit && (flags() & ItemIsMovable))
        emit positionChangedByUser(m_node, sceneCenter());
}

} // namespace mindflow

#include "NodeItem.moc"
