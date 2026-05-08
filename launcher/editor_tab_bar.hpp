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
    /// Emitted when "Merge Split" is selected from the context menu
    /// (only available when setSplitActive(true) has been called).
    void mergeRequested();

public:
    /// Enable or disable split-pane mode in the context menu.
    /// When active, "Split Right" is replaced by "Merge Split".
    void setSplitActive(bool active);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int m_contextTabIndex = -1;
    int m_dragTabIndex = -1;
    QPoint m_dragStartPos;
    bool m_dragging = false;
    bool m_splitActive = false;
    static constexpr int DRAG_THRESHOLD = 10;
};
