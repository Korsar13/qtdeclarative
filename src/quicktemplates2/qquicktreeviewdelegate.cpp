// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qquicktreeviewdelegate_p.h"

#include <QtQuickTemplates2/private/qquickitemdelegate_p_p.h>
#include <QtQuick/private/qquicktaphandler_p.h>
#include <QtQuick/private/qquicktreeview_p_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype TreeViewDelegate
    \inherits ItemDelegate
    \inqmlmodule QtQuick.Controls
    \since 6.3
    \ingroup qtquickcontrols2-delegates
    \brief A delegate that can be assigned to a TreeView.

    \image qtquickcontrols2-treeviewdelegate.png

    A TreeViewDelegate is a delegate that can be assigned to the
    \l {TableView::delegate} {delegate property} of a \l TreeView.
    It renders the tree, as well as the other columns, in the view
    using the application style.

    \code
    TreeView {
        anchors.fill: parent
        delegate: TreeViewDelegate {}
        // The model needs to be a QAbstractItemModel
        // model: yourTreeModel
    }
    \endcode

    TreeViewDelegate inherits \l ItemDelegate, which means that
    it's composed of three items: a \l[QML]{Control::}{background},
    a \l [QML]{Control::}{contentItem}, and an
    \l [QML]{AbstractButton::}{indicator}. TreeViewDelegate takes care
    of \l {indentation}{indenting} the contentItem and the indicator according
    their location in the tree. The indicator will only be visible if the
    delegate item is inside the \l {isTreeNode}{tree column}, and renders
    a model item \l {hasChildren}{with children}.

    If you change the indicator, it will no longer be indented by default.
    Instead you need to indent it yourself by setting the
    \l [QML] {Item::x}{x position} of the indicator, taking the \l depth and
    \l indentation into account. Below is an example of how to do that:

    \code
    TreeViewDelegate {
        indicator: Item {
            x: leftMargin + (depth * indentation)
        }
    }
    \endcode

    The position of the contentItem is controlled with \l [QML]{Control::}{padding}.
    This means that you can change the contentItem without dealing with indentation.
    But it also means that you should avoid changing padding to something else, as
    that will remove the indentation. The space to the left of the indicator is instead
    controlled with \l leftMargin. The space between the indicator and the contentItem
    is controlled with \l [QML]{Control::}{spacing}. And the space to the right of the
    contentItem is controlled with \l rightMargin.

    \section2 Interacting with pointers
    TreeViewDelegate inherits \l ItemDelegate. This means that it will emit signals
    such as \l {AbstractButton::clicked()}{clicked} when the user clicks on the delegate.
    If needed, you could connect to that signal to implement application specific
    functionality, in addition to the default expand/collapse behavior (and even set \l
    {QQuickTableView::pointerNavigationEnabled}{pointerNavigationEnabled} to \c false, to
    disable the default behavior as well).

    But the ItemDelegate API does not give you information about the position of the
    click, or which modifiers are being held. If this is needed, a better approach would
    be to use pointer handlers. To ensure that they don't interfere with the
    existing logic in TreeViewDelegate, install them on a child item, e.g:

    \code
    TreeView {
        id: treeView
        delegate: TreeViewDelegate {
            Item {
                anchors.fill: parent
                TapHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onTapped: {
                        if (treeView.isExpanded(row))
                            treeView.collapseRecursively(row)
                        else
                            treeView.expandRecursively(row)
                    }
                }
            }
        }
    }
    \endcode

    \sa TreeView
*/

/*!
    \qmlproperty TreeView QtQuick.Controls::TreeViewDelegate::treeView

    This property points to the \l TreeView that contains the delegate item.
*/

/*!
    \qmlproperty bool QtQuick.Controls::TreeViewDelegate::hasChildren

    This property is \c true if the model item drawn by the delegate
    has children in the model.
*/

/*!
    \qmlproperty bool QtQuick.Controls::TreeViewDelegate::isTreeNode

    This property is \c true if the delegate item draws a node in the tree.
    Only one column in the view will be used to to draw the tree, and
    therefore, only delegate items in that column will have this
    property set to \c true.

    A node in the tree is \l{indentation}{indented} according to its \c depth, and show
    an \l [QML]{AbstractButton::}{indicator} if hasChildren is \c true. Delegate items
    in other columns will have this property set to \c false.
*/

/*!
    \qmlproperty bool QtQuick.Controls::TreeViewDelegate::expanded

    This property is \c true if the model item drawn by the delegate
    is expanded in the view.
*/

/*!
    \qmlproperty int QtQuick.Controls::TreeViewDelegate::depth

    This property holds the depth of the model item drawn by the delegate.
    The depth of a model item is the same as the number of ancestors
    it has in the model.
*/

/*!
    \qmlproperty bool QtQuick.Controls::TreeViewDelegate::current

    This property holds if the delegate represent the
    \l {QItemSelectionModel::currentIndex()}{current index}
    in the \l {QQuickTableView::selectionModel()}{selection model}.
*/

/*!
    \qmlproperty bool QtQuick.Controls::TreeViewDelegate::selected

    This property holds if the delegate represent a
    \l {QItemSelectionModel::selection()}{selected index}
    in the \l {QQuickTableView::selectionModel()}{selection model}.
*/

/*!
    \qmlproperty real QtQuick.Controls::TreeViewDelegate::indentation

    This property holds the space a child is indented horizontally
    relative to its parent.
*/

/*!
    \qmlproperty real QtQuick.Controls::TreeViewDelegate::leftMargin

    This property holds the space between the left edge of the view
    and the left edge of the indicator (in addition to indentation).
    If no indicator is visible, the space will be between the left
    edge of the view and the left edge of the contentItem.

    \sa rightMargin, indentation, {QQuickControl::}{spacing}
*/

/*!
    \qmlproperty real QtQuick.Controls::TreeViewDelegate::rightMargin

    This property holds the space between the right edge of the view
    and the right edge of the contentItem.

    \sa leftMargin, indentation, {QQuickControl::}{spacing}
*/

class QQuickTreeViewDelegatePrivate : public QQuickItemDelegatePrivate
{
public:
    Q_DECLARE_PUBLIC(QQuickTreeViewDelegate)

    void updateIndicatorVisibility();
    QPalette defaultPalette() const override;
    bool posOnTopOfIndicator(const QPointF &pos);
    void handleClickOnIndicator(QMouseEvent *event, bool isPress);
    void setCurrentIndex(const QPointF pos);

public:
    QPointer<QQuickTreeView> m_treeView;
    qreal m_indentation = 18;
    qreal m_leftMargin = 0;
    qreal m_rightMargin = 0;
    bool m_isTreeNode = false;
    bool m_expanded = false;
    bool m_current = false;
    bool m_selected = false;
    bool m_hasChildren = false;
    bool m_pressOnTopOfIndicator = false;
    int m_depth = 0;
};

void QQuickTreeViewDelegatePrivate::updateIndicatorVisibility()
{
    Q_Q(QQuickTreeViewDelegate);

    if (auto indicator = q_func()->indicator()) {
        const bool insideDelegateBounds = indicator->x() + indicator->width() < q->width();
        indicator->setVisible(m_isTreeNode && m_hasChildren && insideDelegateBounds);
    }
}

bool QQuickTreeViewDelegatePrivate::posOnTopOfIndicator(const QPointF &pos)
{
    Q_Q(QQuickTreeViewDelegate);

    const auto indicator = q->indicator();
    if (!indicator || !indicator->isVisible())
        return false;

    const auto posInIndicator = q->mapToItem(indicator, pos);
    if (!indicator->contains(posInIndicator))
        return false;

    return true;
}

void QQuickTreeViewDelegatePrivate::handleClickOnIndicator(QMouseEvent *event, bool isPress)
{
    Q_Q(QQuickTreeViewDelegate);

    event->accept();

    // To not interfere with flicking, we only toggle expanded on press
    // if the flickable is not interactive. Otherwise we do it on release.
    const bool interactOnRelease = q->treeView()->isInteractive();
    if (isPress == interactOnRelease)
        return;

    auto view = q->treeView();
    if (!view)
        return;

    const int row = qmlContext(q)->contextProperty(QStringLiteral("row")).toInt();
    view->toggleExpanded(row);
}

void QQuickTreeViewDelegatePrivate::setCurrentIndex(const QPointF pos)
{
    Q_Q(QQuickTreeViewDelegate);

    auto view = q->treeView();
    if (!view)
        return;

    if (!view->pointerNavigationEnabled())
        return;

    const auto posOnContentItem = q->mapToItem(view->contentItem(), pos);
    const QPoint cell = view->cellAtPosition(posOnContentItem);
    if (cell.x() == -1 && cell.y() == -1)
        return;

    QItemSelectionModel *model = view->selectionModel();
    if (!model)
        return;

    model->setCurrentIndex(view->modelIndex(cell), QItemSelectionModel::NoUpdate);
    view->positionViewAtCell(cell, QQuickTableView::Contain);
    view->forceActiveFocus();
}

QQuickTreeViewDelegate::QQuickTreeViewDelegate(QQuickItem *parent)
    : QQuickItemDelegate(*(new QQuickTreeViewDelegatePrivate), parent)
{
}

void QQuickTreeViewDelegate::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(QQuickTreeViewDelegate);

    QQuickAbstractButton::geometryChange(newGeometry, oldGeometry);
    d->updateIndicatorVisibility();
}

void QQuickTreeViewDelegate::mousePressEvent(QMouseEvent *event)
{
    Q_D(QQuickTreeViewDelegate);

    if (d->m_treeView && !d->m_treeView->pointerNavigationEnabled()) {
        QQuickAbstractButton::mousePressEvent(event);
        return;
    }

    d->m_pressOnTopOfIndicator = d->posOnTopOfIndicator(event->position());
    if (d->m_pressOnTopOfIndicator) {
        d->handleClickOnIndicator(event, true);
        return;
    }

    const bool interactOnPress = !treeView()->isInteractive();
    if (interactOnPress && contains(event->position())) {
        d->setCurrentIndex(event->position());
        if (d->m_treeView && d->m_treeView->selectionModel())
            d->m_treeView->selectionModel()->clearSelection();
    }

    QQuickAbstractButton::mousePressEvent(event);
}

void QQuickTreeViewDelegate::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(QQuickTreeViewDelegate);

    if (d->m_pressOnTopOfIndicator) {
        if (d->posOnTopOfIndicator(event->position()))
            d->handleClickOnIndicator(event, false);
        return;
    }

    const bool interactOnRelease = treeView()->isInteractive();
    if (interactOnRelease && contains(event->position())) {
        d->setCurrentIndex(event->position());
        if (d->m_treeView && d->m_treeView->selectionModel())
            d->m_treeView->selectionModel()->clearSelection();
    }

    QQuickAbstractButton::mouseReleaseEvent(event);
}

void QQuickTreeViewDelegate::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(QQuickTreeViewDelegate);

    if (d->m_treeView && !d->m_treeView->pointerNavigationEnabled()) {
        QQuickAbstractButton::mouseDoubleClickEvent(event);
        return;
    }

    event->accept();

    if (d->posOnTopOfIndicator(event->position()))
        return;

    if (auto view = treeView()) {
        const int row = qmlContext(this)->contextProperty(QStringLiteral("row")).toInt();
        view->toggleExpanded(row);
    }

    QQuickAbstractButton::mouseDoubleClickEvent(event);
}

QPalette QQuickTreeViewDelegatePrivate::defaultPalette() const
{
    return QQuickTheme::palette(QQuickTheme::ItemView);
}

QFont QQuickTreeViewDelegate::defaultFont() const
{
    return QQuickTheme::font(QQuickTheme::ItemView);
}

qreal QQuickTreeViewDelegate::indentation() const
{
    return d_func()->m_indentation;
}

void QQuickTreeViewDelegate::setIndentation(qreal indentation)
{
    Q_D(QQuickTreeViewDelegate);
    if (qFuzzyCompare(d->m_indentation, indentation))
        return;

    d->m_indentation = indentation;
    emit indentationChanged();
}

bool QQuickTreeViewDelegate::isTreeNode() const
{
    return d_func()->m_isTreeNode;
}

void QQuickTreeViewDelegate::setIsTreeNode(bool isTreeNode)
{
    Q_D(QQuickTreeViewDelegate);
    if (d->m_isTreeNode == isTreeNode)
        return;

    d->m_isTreeNode = isTreeNode;
    d->updateIndicatorVisibility();
    emit isTreeNodeChanged();
}

bool QQuickTreeViewDelegate::hasChildren() const
{
    return d_func()->m_hasChildren;
}

void QQuickTreeViewDelegate::setHasChildren(bool hasChildren)
{
    Q_D(QQuickTreeViewDelegate);
    if (d->m_hasChildren == hasChildren)
        return;

    d->m_hasChildren = hasChildren;
    d->updateIndicatorVisibility();
    emit hasChildrenChanged();
}

bool QQuickTreeViewDelegate::expanded() const
{
    return d_func()->m_expanded;
}

void QQuickTreeViewDelegate::setExpanded(bool expanded)
{
    Q_D(QQuickTreeViewDelegate);
    if (d->m_expanded == expanded)
        return;

    d->m_expanded = expanded;
    emit expandedChanged();
}

bool QQuickTreeViewDelegate::current() const
{
    return d_func()->m_current;
}

void QQuickTreeViewDelegate::setCurrent(bool current)
{
    Q_D(QQuickTreeViewDelegate);
    if (d->m_current == current)
        return;

    d->m_current = current;
    emit currentChanged();
}

bool QQuickTreeViewDelegate::selected() const
{
    return d_func()->m_selected;
}

void QQuickTreeViewDelegate::setSelected(bool selected)
{
    Q_D(QQuickTreeViewDelegate);
    if (d->m_selected == selected)
        return;

    d->m_selected = selected;
    emit selectedChanged();
}

int QQuickTreeViewDelegate::depth() const
{
    return d_func()->m_depth;
}

void QQuickTreeViewDelegate::setDepth(int depth)
{
    Q_D(QQuickTreeViewDelegate);
    if (d->m_depth == depth)
        return;

    d->m_depth = depth;
    emit depthChanged();
}

QQuickTreeView *QQuickTreeViewDelegate::treeView() const
{
    return d_func()->m_treeView;
}

void QQuickTreeViewDelegate::setTreeView(QQuickTreeView *treeView)
{
    Q_D(QQuickTreeViewDelegate);
    if (d->m_treeView == treeView)
        return;

    d->m_treeView = treeView;
    emit treeviewChanged();
}

void QQuickTreeViewDelegate::componentComplete()
{
    QQuickAbstractButton::componentComplete();
    d_func()->updateIndicatorVisibility();
}

qreal QQuickTreeViewDelegate::leftMargin() const
{
    return d_func()->m_leftMargin;
}

void QQuickTreeViewDelegate::setLeftMargin(qreal leftMargin)
{
    Q_D(QQuickTreeViewDelegate);
    if (qFuzzyCompare(d->m_leftMargin, leftMargin))
        return;

    d->m_leftMargin = leftMargin;
    emit leftMarginChanged();
}

qreal QQuickTreeViewDelegate::rightMargin() const
{
    return d_func()->m_rightMargin;
}

void QQuickTreeViewDelegate::setRightMargin(qreal rightMargin)
{
    Q_D(QQuickTreeViewDelegate);
    if (qFuzzyCompare(d->m_rightMargin, rightMargin))
        return;

    d->m_rightMargin = rightMargin;
    emit rightMarginChanged();
}

QT_END_NAMESPACE

#include "moc_qquicktreeviewdelegate_p.cpp"
