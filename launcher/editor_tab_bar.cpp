#include "editor_tab_bar.hpp"
#include "theme.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>

EditorTabBar::EditorTabBar(QWidget *parent)
    : QTabBar(parent)
{
    setMovable(true);
    setTabsClosable(true);
    setExpanding(false);
    setDrawBase(false);

    setStyleSheet(QString(R"(
        QTabBar::tab {
            background-color: %1; color: %2; padding: 6px 14px;
            border: none; border-right: 1px solid %3;
            font-size: 12px; min-width: 80px;
        }
        QTabBar::tab:selected {
            background-color: %4; color: %5;
            border-bottom: 2px solid %6;
        }
        QTabBar::tab:hover:!selected {
            background-color: %7;
        }
    )").arg(Theme::BgSurface)
       .arg(Theme::TextMuted)
       .arg(Theme::BorderDivider)
       .arg(Theme::BgBase)
       .arg(Theme::TextPrimary)
       .arg(Theme::Gold)
       .arg(Theme::BgSurfaceHover));
}

void EditorTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    m_contextTabIndex = tabAt(event->pos());

    QMenu menu(this);
    menu.setStyleSheet(Theme::menuStyle());

    QAction *closeAction = menu.addAction("Close");
    QAction *closeOthersAction = menu.addAction("Close Others");
    QAction *closeAllAction = menu.addAction("Close All");
    menu.addSeparator();
    QAction *copyPathAction = menu.addAction("Copy Path");
    menu.addSeparator();
    QAction *splitAction = menu.addAction("Split Right");

    bool onTab = m_contextTabIndex >= 0;
    closeAction->setEnabled(onTab);
    closeOthersAction->setEnabled(onTab && count() > 1);
    copyPathAction->setEnabled(onTab);

    QAction *selected = menu.exec(event->globalPos());

    if (selected == closeAction) {
        emit tabCloseRequested(m_contextTabIndex);
    } else if (selected == closeOthersAction) {
        // Close right-to-left so indices remain stable for earlier tabs
        for (int i = count() - 1; i >= 0; --i) {
            if (i != m_contextTabIndex)
                emit tabCloseRequested(i);
        }
    } else if (selected == closeAllAction) {
        for (int i = count() - 1; i >= 0; --i)
            emit tabCloseRequested(i);
    } else if (selected == copyPathAction) {
        QString path = tabData(m_contextTabIndex).toString();
        if (!path.isEmpty())
            QApplication::clipboard()->setText(path);
    } else if (selected == splitAction) {
        emit splitRightRequested();
    }
}

void EditorTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        m_dragging = false;
    }
    QTabBar::mousePressEvent(event);
}

void EditorTabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        int dist = (event->pos() - m_dragStartPos).manhattanLength();
        if (dist > DRAG_THRESHOLD && !m_dragging) {
            m_dragging = true;
        }
    }
    QTabBar::mouseMoveEvent(event);
}
