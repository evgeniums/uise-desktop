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

        bool isLineUnderMouse() const;

    private slots:

        void onWidgetDestroyed();

    private:

        QWidget* m_content=nullptr;
        QHBoxLayout* m_layout=nullptr;
        HTreeSplitterLine* m_line=nullptr;
        QWidget* m_widget=nullptr;
};

class HTreeSplitterInternal : public QFrame
{
    Q_OBJECT

    public:

        HTreeSplitterInternal(QWidget* parent=nullptr);

        ~HTreeSplitterInternal();

        void addWidget(QWidget* widget, int stretch=0);
        QWidget* widget(int index) const;

        int count() const;

    signals:

        void minMaxSizeUpdated();

    private slots:

        void onSectionDestroyed(QObject* obj);

    protected:

        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;

    private:

        struct Section
        {
            int minWidth=0;
            QObject* obj=nullptr;
        };

        QHBoxLayout* m_layout;
        std::vector<Section> m_sections;

        QPoint m_prevMousePos;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_SPLITTER_HPP
