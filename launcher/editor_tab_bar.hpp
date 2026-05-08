#pragma once

#include <QTabBar>
#include <QPoint>

/// Custom tab bar for the script editor.
/// Adds right-click context menu (Close, Close Others, Close All, Copy Path)
/// and drag detection for tab-to-split operations.
class EditorTabBar : public QTabBar {
    Q_OBJECT
public:
    explicit EditorTabBar(QWidget *parent = nullptr);

signals:
    /// Emitted when the user right-clicks and selects "Split Right",
    /// or when a tab is dragged far enough to initiate a split.
    void splitRightRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    int m_contextTabIndex = -1;
    QPoint m_dragStartPos;
    bool m_dragging = false;
    static constexpr int DRAG_THRESHOLD = 10;
};
