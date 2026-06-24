#include "canvas/MindMapView.h"

#include "canvas/BranchItem.h"
#include "canvas/ConnectionItem.h"
#include "canvas/NodeItem.h"
#include "model/Document.h"
#include "model/Node.h"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QGraphicsScene>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <functional>

namespace mindflow {

MindMapView::MindMapView(QWidget* parent) : QGraphicsView(parent) {
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-5000, -5000, 10000, 10000);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setBackgroundBrush(QColor(0xf5, 0xf6, 0xf8));
    setFocusPolicy(Qt::StrongFocus);

    connect(m_scene, &QGraphicsScene::selectionChanged, this,
            [this] { emit selectionChanged(selectedNode()); });

    // Real keyboard input is delivered to the viewport; filter it so canvas command
    // keys are handled no matter which widget the windowing system targets.
    viewport()->installEventFilter(this);

    m_anim = new QVariantAnimation(this);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setDuration(220);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        const double t = v.toDouble();
        for (auto it = m_animTarget.constBegin(); it != m_animTarget.constEnd(); ++it) {
            NodeItem* item = it.key();
            const QPointF start = m_animStart.value(item, it.value());
            item->setTopLeft(start + (it.value() - start) * t);
        }
        updateConnections();
    });
    connect(m_anim, &QVariantAnimation::finished, this, [this] {
        for (auto it = m_animTarget.constBegin(); it != m_animTarget.constEnd(); ++it)
            it.key()->setTopLeft(it.value());
        updateConnections();
    });
}

void MindMapView::setDocument(Document* doc) {
    if (m_doc)
        m_doc->disconnect(this);
    m_doc = doc;
    if (m_doc) {
        connect(m_doc, &Document::structureChanged, this,
                &MindMapView::onStructureChanged);
        connect(m_doc, &Document::nodeChanged, this, &MindMapView::onNodeChanged);
    }
    rebuild();
    zoomToFit();
}

// ----------------------------------------------------------------- rebuild ----
void MindMapView::rebuild() {
    // Snapshot existing positions by node id so surviving nodes can glide from
    // where they were (and new nodes can emerge from their parent) when animating.
    m_anim->stop();
    QHash<int, QPointF> oldPos;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it)
        oldPos.insert(it.key()->id(), it.value()->pos());
    // Preserve the current selection across the rebuild so the inspector keeps
    // tracking the same node (e.g. after a theme recolor or content edit).
    Node* selected = selectedNode();
    const int selectedId = selected ? selected->id() : -1;

    m_nodeItems.clear();
    m_branches.clear();
    m_connectionItems.clear();
    m_animStart.clear();
    m_animTarget.clear();
    // Tear down old items. This is typically called from inside a NodeItem's own
    // event handler (Tab/Enter/Delete/…), so deleting the acting item immediately
    // would be a use-after-free once control returns to Qt's event dispatch. Use
    // deleteLater for QObject items so they outlive the current event. First drop
    // the scene's focus item explicitly: otherwise, when the previously-focused
    // item is finally deleted, its destructor would clear the focus we are about to
    // give the new node's inline editor.
    m_scene->setFocusItem(nullptr);
    const auto oldItems = m_scene->items();
    for (QGraphicsItem* gi : oldItems) {
        if (gi->parentItem())
            continue; // child items (e.g. the inline editor) go with their parent
        m_scene->removeItem(gi);
        if (auto* obj = dynamic_cast<QObject*>(gi))
            obj->deleteLater();
        else
            delete gi;
    }
    if (!m_doc || !m_doc->root())
        return;

    // Create node items, skipping subtrees hidden under a collapsed node.
    std::function<void(Node*)> makeItems = [&](Node* node) {
        auto* item = new NodeItem(node);
        m_scene->addItem(item);
        m_nodeItems.insert(node, item);
        wireItem(item);
        if (node->collapsed)
            return;
        for (const auto& c : node->children())
            makeItems(c.get());
    };
    for (const auto& root : m_doc->roots())
        makeItems(root.get());

    // Create a branch per visible parent-child edge.
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        Node* node = it.key();
        if (node->collapsed)
            continue;
        for (const auto& c : node->children()) {
            if (!m_nodeItems.contains(c.get()))
                continue;
            auto* branch = new BranchItem();
            branch->setColor(c->style.fillColor);
            branch->setConnector(c->style.connector);
            m_scene->addItem(branch);
            m_branches.append(branch);
        }
    }

    // Cross-connections (skip any whose endpoints no longer exist).
    for (const Connection& conn : m_doc->connections()) {
        if (!m_doc->nodeById(conn.fromId) || !m_doc->nodeById(conn.toId))
            continue;
        auto* item = new ConnectionItem(conn.id);
        item->setWaypointOffset(conn.waypointOffset);
        item->setLabel(conn.label);
        item->setVisible(m_connectionsVisible);
        m_scene->addItem(item);
        m_connectionItems.append(item);
        wireConnection(item);
    }

    // Seed initial positions: surviving nodes keep their old spot; new nodes start
    // at their parent's old spot so they animate outward.
    const bool canAnimate = !oldPos.isEmpty();
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        Node* node = it.key();
        NodeItem* item = it.value();
        if (oldPos.contains(node->id())) {
            item->setTopLeft(oldPos.value(node->id()));
        } else if (node->parent() && oldPos.contains(node->parent()->id())) {
            item->setTopLeft(oldPos.value(node->parent()->id()));
        }
    }

    relayout(/*animate=*/canAnimate);
    applyEmphasis();

    // Restore the prior selection so the inspector keeps tracking the same node.
    // (Freshly added nodes are selected/edited by the add handlers after this.)
    if (selectedId >= 0) {
        if (Node* n = m_doc->nodeById(selectedId))
            if (NodeItem* item = itemFor(n))
                item->setSelected(true);
    }
}

// ---------------------------------------------------------------- relayout ----
QHash<NodeItem*, QPointF> MindMapView::computeTargets() const {
    QHash<NodeItem*, QPointF> targets;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        Node* node = it.key();
        NodeItem* item = it.value();
        const QPointF center = node->layoutPos;
        const QSizeF s = item->size();
        targets.insert(item, center - QPointF(s.width() / 2.0, s.height() / 2.0));
    }
    return targets;
}

void MindMapView::relayout(bool animate) {
    if (!m_doc || !m_doc->root())
        return;

    LayoutEngine::SizeProvider sizeOf = [this](const Node* n) -> QSizeF {
        if (NodeItem* item = m_nodeItems.value(const_cast<Node*>(n), nullptr))
            return item->size();
        return QSizeF(80, 28);
    };
    for (const auto& root : m_doc->roots())
        m_layout.layout(root.get(), sizeOf);

    const QHash<NodeItem*, QPointF> targets = computeTargets();
    if (animate) {
        m_anim->stop();
        m_animTarget = targets;
        m_animStart.clear();
        for (auto it = targets.constBegin(); it != targets.constEnd(); ++it)
            m_animStart.insert(it.key(), it.key()->pos());
        m_anim->start();
    } else {
        for (auto it = targets.constBegin(); it != targets.constEnd(); ++it)
            it.key()->setTopLeft(it.value());
        updateConnections();
    }
}

void MindMapView::updateConnections() {
    // Re-point branches from parent edge to child edge, in stable iteration order.
    int bi = 0;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        Node* node = it.key();
        NodeItem* parentItem = it.value();

        // Fold knob hint: does this node have children, and on which side?
        if (!node->children().empty()) {
            Node* firstChild = node->children().front().get();
            NodeItem* childItem = m_nodeItems.value(firstChild, nullptr);
            const QPointF childCenter =
                childItem ? childItem->sceneCenter()
                          : parentItem->sceneCenter() + QPointF(1, 0);
            parentItem->setFoldHint(true, childCenter);
        } else {
            parentItem->setFoldHint(false, QPointF());
        }

        if (node->collapsed)
            continue;
        for (const auto& c : node->children()) {
            NodeItem* childItem = m_nodeItems.value(c.get(), nullptr);
            if (!childItem || bi >= m_branches.size())
                continue;
            m_branches[bi++]->setEndpoints(parentItem->sceneCenter(),
                                           childItem->sceneCenter());
        }
    }

    // Cross-connection endpoints.
    if (m_doc) {
        for (ConnectionItem* item : m_connectionItems) {
            const Connection* c = m_doc->connectionById(item->connectionId());
            if (!c)
                continue;
            NodeItem* a = m_doc->nodeById(c->fromId) ? itemFor(m_doc->nodeById(c->fromId)) : nullptr;
            NodeItem* b = m_doc->nodeById(c->toId) ? itemFor(m_doc->nodeById(c->toId)) : nullptr;
            if (a && b)
                item->setEndpoints(a->sceneCenter(), b->sceneCenter());
        }
    }
}

void MindMapView::wireItem(NodeItem* item) {
    connect(item, &NodeItem::textCommitted, this, &MindMapView::handleTextCommitted);
    connect(item, &NodeItem::positionChangedByUser, this,
            &MindMapView::handlePositionChanged);
    connect(item, &NodeItem::addChildRequested, this, &MindMapView::handleAddChild);
    connect(item, &NodeItem::addSiblingRequested, this, &MindMapView::handleAddSibling);
    connect(item, &NodeItem::deleteRequested, this, &MindMapView::handleDelete);
    connect(item, &NodeItem::toggleCollapseRequested, this,
            &MindMapView::handleToggleCollapse);
    connect(item, &NodeItem::toggleTaskRequested, this, &MindMapView::handleToggleTask);
    connect(item, &NodeItem::navigateRequested, this, &MindMapView::handleNavigate);
    // Live-follow branches while a node is dragged (skip during animation, which
    // already refreshes connections once per frame).
    connect(item, &NodeItem::geometryChanged, this, [this] {
        if (m_anim->state() != QAbstractAnimation::Running)
            updateConnections();
    });
}

void MindMapView::wireConnection(ConnectionItem* item) {
    connect(item, &ConnectionItem::waypointDragged, this,
            [this](int id, const QPointF& offset) {
                if (m_doc)
                    m_doc->setConnectionWaypoint(id, offset);
            });
    connect(item, &ConnectionItem::straightenRequested, this, [this](int id) {
        if (m_doc)
            m_doc->setConnectionWaypoint(id, QPointF(0, 0));
    });
    connect(item, &ConnectionItem::deleteRequested, this, [this](int id) {
        if (m_doc)
            m_doc->removeConnection(id);
    });
    connect(item, &ConnectionItem::labelEditRequested, this, [this](int id) {
        if (!m_doc)
            return;
        Connection* c = m_doc->connectionById(id);
        if (!c)
            return;
        bool ok = false;
        const QString text = QInputDialog::getText(
            this, tr("Connection Label"), tr("Label:"), QLineEdit::Normal, c->label, &ok);
        if (ok)
            m_doc->setConnectionLabel(id, text);
    });
}

// ------------------------------------------------------- model-signal slots ----
void MindMapView::onStructureChanged() { rebuild(); }

void MindMapView::onNodeChanged(Node* node) {
    if (NodeItem* item = itemFor(node)) {
        item->refresh();
        relayout(); // size may have changed
    }
}

// -------------------------------------------------------- interaction slots ----
//
// Invoked synchronously from a NodeItem/ConnectionItem event handler. The rebuild()
// these trigger deletes the acting item, but rebuild() uses deleteLater() so the
// item survives until the current event returns (no use-after-free). Adding a node
// then selecting+editing it in one synchronous step keeps keyboard entry crisp:
// the inline editor opens immediately, so the next keystrokes land in it.
void MindMapView::handleAddChild(Node* parent) {
    if (!m_doc || !parent)
        return;
    if (Node* added = m_doc->addChild(parent, QString()))
        selectAndMaybeEdit(added, /*edit=*/true);
}

void MindMapView::handleAddSibling(Node* node) {
    if (!m_doc || !node)
        return;
    if (Node* added = m_doc->addSibling(node, QString()))
        selectAndMaybeEdit(added, /*edit=*/true);
}

void MindMapView::handleDelete(Node* node) {
    if (m_doc)
        m_doc->removeNode(node);
}

void MindMapView::handleTextCommitted(Node* node, const QString& text) {
    if (m_doc)
        m_doc->setNodeText(node, text);
}

void MindMapView::handlePositionChanged(Node* node, const QPointF& sceneCenter) {
    if (m_doc)
        m_doc->setNodePosition(node, sceneCenter);
}

void MindMapView::handleToggleCollapse(Node* node) {
    if (m_doc)
        m_doc->setCollapsed(node, !node->collapsed);
}

void MindMapView::handleToggleTask(Node* node) {
    if (m_doc)
        m_doc->toggleTaskDone(node);
}

void MindMapView::handleNavigate(Node* from, int key) {
    NodeItem* fromItem = itemFor(from);
    if (!fromItem)
        return;
    const QPointF f = fromItem->sceneCenter();

    // Pick the nearest node within a 90° cone in the pressed direction. This is
    // layout-agnostic: Right always goes right on screen, etc. (No model rebuild,
    // so it's safe to run synchronously inside the key event.)
    NodeItem* best = nullptr;
    double bestDist = 1e18;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        NodeItem* ni = it.value();
        if (ni == fromItem)
            continue;
        const QPointF c = ni->sceneCenter();
        const double ddx = c.x() - f.x();
        const double ddy = c.y() - f.y();
        const double adx = qAbs(ddx);
        const double ady = qAbs(ddy);
        bool ok = false;
        switch (key) {
        case Qt::Key_Right: ok = ddx > 0 && adx >= ady; break;
        case Qt::Key_Left:  ok = ddx < 0 && adx >= ady; break;
        case Qt::Key_Down:  ok = ddy > 0 && ady >= adx; break;
        case Qt::Key_Up:    ok = ddy < 0 && ady >= adx; break;
        default: break;
        }
        if (!ok)
            continue;
        const double dist = ddx * ddx + ddy * ddy;
        if (dist < bestDist) {
            bestDist = dist;
            best = ni;
        }
    }
    if (best) {
        m_scene->clearSelection();
        best->setSelected(true);
        best->setFocus(); // keep keyboard focus moving with the selection
        ensureVisible(best, 60, 60);
    }
}

void MindMapView::setTagHighlight(const QString& tag) {
    m_highlightTag = tag;
    applyEmphasis();
}

bool MindMapView::inFocusSubtree(Node* node) const {
    if (m_focusNodeId < 0)
        return true;
    for (Node* n = node; n; n = n->parent())
        if (n->id() == m_focusNodeId)
            return true;
    return false;
}

void MindMapView::applyEmphasis() {
    const bool tagActive = !m_highlightTag.isEmpty();
    const bool focusActive = m_focusNodeId >= 0;
    const bool anyActive = tagActive || focusActive;

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        Node* node = it.key();
        const bool tagOk = !tagActive || node->tags.contains(m_highlightTag);
        const bool focusOk = !focusActive || inFocusSubtree(node);
        it.value()->setOpacity(tagOk && focusOk ? 1.0 : 0.12);
    }
    for (BranchItem* b : m_branches)
        b->setOpacity(anyActive ? 0.12 : 1.0);
    for (ConnectionItem* c : m_connectionItems)
        c->setOpacity(anyActive ? 0.12 : 1.0);
}

QList<Node*> MindMapView::selectedNodes() const {
    QList<Node*> out;
    for (QGraphicsItem* it : m_scene->selectedItems())
        if (auto* ni = qgraphicsitem_cast<NodeItem*>(it))
            out.append(ni->node());
    return out;
}

void MindMapView::connectSelection() {
    const QList<Node*> sel = selectedNodes();
    if (sel.size() == 2 && m_doc)
        m_doc->addConnection(sel[0], sel[1]);
}

void MindMapView::setConnectionsVisible(bool on) {
    m_connectionsVisible = on;
    for (ConnectionItem* c : m_connectionItems)
        c->setVisible(on);
}

void MindMapView::toggleFocusOnSelection() {
    Node* n = selectedNode();
    if (!n) {
        clearFocus();
        return;
    }
    m_focusNodeId = (m_focusNodeId == n->id()) ? -1 : n->id();
    applyEmphasis();
}

void MindMapView::clearFocus() {
    m_focusNodeId = -1;
    applyEmphasis();
}

void MindMapView::selectNode(Node* node) {
    NodeItem* item = node ? itemFor(node) : nullptr;
    if (!item)
        return;
    m_scene->clearSelection();
    item->setSelected(true);
    // Reveal the node if it's off-screen, but don't recenter when it's already visible.
    ensureVisible(item, 60, 60);
}

void MindMapView::searchAndSelect(const QString& query) {
    if (query.isEmpty() || !m_doc)
        return;
    m_scene->clearSelection();
    NodeItem* first = nullptr;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        if (it.key()->text().contains(query, Qt::CaseInsensitive)) {
            it.value()->setSelected(true);
            if (!first)
                first = it.value();
        }
    }
    if (first) {
        centerOn(first);
        first->setFocus();
    }
}

// ------------------------------------------------------------- public slots ----
void MindMapView::addChildToSelection() {
    if (Node* n = selectedNode())
        handleAddChild(n);
    else if (m_doc)
        handleAddChild(m_doc->root());
}

void MindMapView::editSelection() {
    if (Node* n = selectedNode())
        selectAndMaybeEdit(n, true);
}

void MindMapView::deleteSelection() {
    if (Node* n = selectedNode())
        handleDelete(n);
}

void MindMapView::toggleCollapseSelection() {
    if (Node* n = selectedNode())
        handleToggleCollapse(n);
}

void MindMapView::detachSelection() {
    Node* n = selectedNode();
    if (!n || !m_doc)
        return;
    NodeItem* item = itemFor(n);
    const QPointF where = item ? item->sceneCenter() : QPointF(0, 0);
    m_doc->detachAsRoot(n, where);
}

void MindMapView::setSelectionLayoutDirection(mindflow::LayoutDirection dir) {
    if (!m_doc)
        return;
    // Layout direction applies per tree, so target the root of the selection.
    Node* n = selectedNode();
    Node* root = n ? n : m_doc->root();
    while (root && root->parent())
        root = root->parent();
    m_doc->setLayoutDirection(root, dir);
}

Node* MindMapView::selectedNode() const {
    NodeItem* item = selectedItem();
    return item ? item->node() : nullptr;
}

NodeItem* MindMapView::selectedItem() const {
    const auto sel = m_scene->selectedItems();
    for (QGraphicsItem* it : sel)
        if (auto* ni = qgraphicsitem_cast<NodeItem*>(it))
            return ni;
    return nullptr;
}

NodeItem* MindMapView::editingItem() const {
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it)
        if (it.value()->isEditing())
            return it.value();
    return nullptr;
}

bool MindMapView::handleCommandKey(QKeyEvent* event) {
    // Target the node being edited if any, else the selection. This is the fix for
    // the "Enter adds a child instead of a sibling" bug: while typing a new node's
    // name the command must act on THAT node, not on whatever the selection state
    // happens to be (which can lag during rapid entry / differ by input route).
    NodeItem* item = editingItem();
    if (!item)
        item = selectedItem();
    if (!item)
        return false;

    Node* n = item->node();
    const bool editing = item->isEditing();
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    switch (event->key()) {
    case Qt::Key_Tab:
        if (editing)
            item->commitEditing();
        handleAddChild(n);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (shift)
            return false; // Shift+Enter -> newline in the editor
        if (editing)
            item->commitEditing();
        handleAddSibling(n);
        return true;
    case Qt::Key_F2:
        if (!editing) {
            item->beginEditing();
            return true;
        }
        return false;
    case Qt::Key_Delete:
        if (!editing) {
            handleDelete(n);
            return true;
        }
        return false;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        if (!editing) {
            handleNavigate(n, event->key());
            return true;
        }
        return false;
    default:
        return false;
    }
}

void MindMapView::keyPressEvent(QKeyEvent* event) {
    if (handleCommandKey(event)) {
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

bool MindMapView::eventFilter(QObject* obj, QEvent* e) {
    // Real keystrokes are delivered to the viewport; catch command keys here so they
    // never get consumed by the viewport's Tab focus-traversal first.
    if (obj == viewport() && e->type() == QEvent::KeyPress) {
        if (handleCommandKey(static_cast<QKeyEvent*>(e)))
            return true;
    }
    return QGraphicsView::eventFilter(obj, e);
}

void MindMapView::selectAndMaybeEdit(Node* node, bool edit) {
    NodeItem* item = itemFor(node);
    if (!item)
        return;
    m_scene->clearSelection();
    item->setSelected(true);
    ensureVisible(item, 60, 60); // a freshly added node should never land off-screen
    if (edit)
        item->beginEditing(); // the inline editor takes keyboard focus
    else
        item->setFocus();
}

// --------------------------------------------------------------- navigation ----
void MindMapView::applyZoom(double factor) {
    const double next = m_zoom * factor;
    if (next < 0.1 || next > 6.0)
        return;
    m_zoom = next;
    scale(factor, factor);
}

void MindMapView::zoomIn() { applyZoom(1.2); }
void MindMapView::zoomOut() { applyZoom(1.0 / 1.2); }

void MindMapView::resetZoom() {
    resetTransform();
    m_zoom = 1.0;
}

void MindMapView::zoomToFit() {
    if (m_nodeItems.isEmpty())
        return;
    QRectF bounds;
    for (auto* item : m_nodeItems)
        bounds = bounds.united(item->sceneBoundingRect());
    if (bounds.isNull())
        return;
    bounds.adjust(-60, -60, 60, 60);
    fitInView(bounds, Qt::KeepAspectRatio);
    // Don't over-magnify a small map (e.g. a lone root would fill the viewport);
    // cap the zoom and re-center instead.
    if (transform().m11() > 1.5) {
        resetTransform();
        scale(1.5, 1.5);
        centerOn(bounds.center());
    }
    m_zoom = transform().m11();
}

void MindMapView::showEvent(QShowEvent* event) {
    QGraphicsView::showEvent(event);
    // The first real fit must wait until the viewport has a size.
    if (!m_initialFitDone) {
        m_initialFitDone = true;
        zoomToFit();
    }
}

bool MindMapView::event(QEvent* e) {
    // QWidget::event() turns a Tab/Backtab key press into focus traversal before
    // keyPressEvent ever sees it. Intercept those here and route them through our
    // own keyboard handling instead.
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
            keyPressEvent(ke);
            if (ke->isAccepted())
                return true;
        }
    }
    return QGraphicsView::event(e);
}

void MindMapView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        applyZoom(event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void MindMapView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton ||
        (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        m_panning = true;
        m_lastPanPoint = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MindMapView::mouseMoveEvent(QMouseEvent* event) {
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastPanPoint;
        m_lastPanPoint = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MindMapView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_panning) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    // A node drag commits a SetPositionCommand (via NodeItem) which triggers a
    // rebuild + relayout, so nothing more is needed here.
}

} // namespace mindflow
