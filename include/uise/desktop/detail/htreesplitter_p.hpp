/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/detail/htreesplitter_p.hpp
*
*  Declares internal details used for splitter for horizontal tree.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_SPLITTER_P_HPP
#define UISE_DESKTOP_HTREE_SPLITTER_P_HPP

#include <vector>
#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/alignedstretchingwidget.hpp>
#include <uise/desktop/htreepath.hpp>

class QHBoxLayout;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class SingleShotTimer;

class HTreeSplitterLine : public QFrame
{
    Q_OBJECT

    public:

        HTreeSplitterLine(QWidget* parent=nullptr);

    protected:

        void enterEvent(QEnterEvent* event) override;
        void leaveEvent(QEvent* event) override;
};

class HTreeSplitterSection : public QFrame
{
    Q_OBJECT

    public:

        HTreeSplitterSection(QWidget* parent=nullptr);

        ~HTreeSplitterSection();

        void setWidget(QWidget* widget);
        QWidget* widget() const;

        /**
         * @brief Recompute this section's minimum width from its current widget.
         *
         * setWidget() snapshots the widget's minimumWidth() once, which is stale whenever the
         * widget gains its real minimum width only after being inserted -- e.g. an HTreeNode
         * that is appended to the splitter before its content is built, whose
         * HTreeNode::setContentWidget() then raises the node's minimum width. Without this the
         * section keeps the content-less floor and the splitter lets the column be squeezed
         * below the width its content needs.
         *
         * @return true if the minimum width actually changed, so callers can skip the geometry
         *         passes when there is nothing to redo.
         */
        bool refreshMinimumWidth();

        bool isLineUnderMouse() const;

        void setExpanded(bool enable)
        {
            m_expanded=enable;
        }

        bool isExpanded() const
        {
            return m_expanded;
        }

        void setSectionVisible(bool enable)
        {
            setVisible(enable);
            m_visible=enable;
            if (!enable)
            {
                setMinimumWidth(0);
            }
        }

        bool isSectionVisible() const
        {
            return m_visible;
        }

    public slots:

        void setLineVisible(bool enable);

    private slots:

        void onWidgetDestroyed();        

    private:

        QWidget* m_content=nullptr;
        QHBoxLayout* m_layout=nullptr;
        HTreeSplitterLine* m_line=nullptr;
        QFrame* m_stubLine=nullptr;
        QWidget* m_widget=nullptr;

        //! Section's own minimum width before the widget's and the line's are added on top of
        //! it, kept so refreshMinimumWidth() can recompute instead of accumulating.
        int m_baseMinWidth=0;

        bool m_expanded=true;
        bool m_visible=true;

        friend class HTreeSplitter;
};

class HTreeSplitter;

class HTreeSplitterInternal : public QFrame
{
    Q_OBJECT

    public:

        HTreeSplitterInternal(HTreeSplitter* splitter, QWidget* parent=nullptr);

        ~HTreeSplitterInternal();

        void addWidget(QWidget* widget, int stretch=0);
        QWidget* widget(int index) const;
        void removeWidget(int index);

        //! Re-read the minimum width of the section holding @a widget and re-run the geometry
        //! passes. Call after a widget already in the splitter changes its minimum width.
        //! @return true if anything changed and the geometry was redone.
        //! @param force redo the geometry passes even when the minimum width is unchanged
        //!        (needed when the widget was hidden during the previous pass, so the section's
        //!        sizeHint() could not see it).
        bool refreshWidgetMinWidth(QWidget* widget, bool force=false);

        int count() const;

        QSize sizeHint() const override;

        void truncate(int index);

    public slots:

        void toggleSectionExpanded(int index, bool expanded, bool visible);

    signals:

        void minMaxSizeUpdated();
        void adjustViewPortRequested(int width);

    private slots:

        void onSectionDestroyed(QObject* obj);
        void updateSize(int newWidth);

    protected:

        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;

        void resizeEvent(QResizeEvent* event) override;

    private:

        int recalculateWidths(int totalWidth);
        void updatePositions();
        void updateWidths();
        void updateMinWidth();

        void recalculateSectionStretch();        

        struct Section
        {
            Section(
                    QObject* obj=nullptr,
                    int width=0,
                    int minWidth=0,
                    int stretch=0
                ) : obj(obj),width(width),minWidth(minWidth),stretch(stretch)
            {}

            QObject* obj=nullptr;
            int width=0;
            int minWidth=0;
            int stretch=0;
            int manualWidth=0;
            bool destroyed=false;
        };

        void updateStretches(Section* section, int stretch=0);
        Section* section(int index) const;

        void debugState(const char* tag) const;
        void paintEvent(QPaintEvent* event) override;

        void splitterResized(QResizeEvent* event, bool withDelayedReadjust=true);

        QHBoxLayout* m_layout;
        std::vector<std::unique_ptr<Section>> m_sections;

        QPoint m_prevMousePos;
        bool m_blockResizeEvent;
        SingleShotTimer* m_blockResizeTimer;
        SingleShotTimer* m_emitMinMaxSizeTimer;
        SingleShotTimer* m_delayedResizeTimer;
        int m_resizingIndex;
        int m_prevViewportWidth;

        HTreeSplitter* m_splitter;
        bool m_stretchLastSection;

        friend class HTreeSplitter;
};

}

#endif // UISE_DESKTOP_HTREE_SPLITTER_HPP
