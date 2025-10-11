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

UISE_DESKTOP_NAMESPACE_BEGIN

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

        void splitterResized(QResizeEvent* event, bool withDelayedReadjust=true);

        QHBoxLayout* m_layout;
        std::vector<std::unique_ptr<Section>> m_sections;

        QPoint m_prevMousePos;
        bool m_blockResizeEvent;
        SingleShotTimer* m_blockResizeTimer;
        SingleShotTimer* m_emitMinMaxSizeTimer;
        int m_resizingIndex;
        int m_prevViewportWidth;

        HTreeSplitter* m_splitter;
        bool m_stretchLastSection;

        friend class HTreeSplitter;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_SPLITTER_HPP
