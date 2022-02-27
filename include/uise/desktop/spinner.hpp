/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

*/

/****************************************************************************/

/** @file uise/desktop/spinner.hpp
*
*  Declares Spinner.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SPINNER_HPP
#define UISE_DESKTOP_SPINNER_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

class QEnterEvent;

UISE_DESKTOP_NAMESPACE_BEGIN

struct SpinnerSection
{
    QList<QWidget*> items;

    SpinnerSection() = default;
    SpinnerSection(const SpinnerSection&) = default;
    SpinnerSection(SpinnerSection&&) = default;
    SpinnerSection& operator=(const SpinnerSection&) = default;
    SpinnerSection& operator=(SpinnerSection&&) = default;

    ~SpinnerSection()
    {
        qDeleteAll(items);
    }

    int itemsWidth=0;
    int barWidth=0;
    QWidget* barLabel=nullptr;
    int currentOffset=0;
    int currentItemIndex=0;

    int width() const noexcept
    {
        return itemsWidth+barWidth;
    }
};

class UISE_DESKTOP_EXPORT Spinner : public QFrame
{
    Q_OBJECT

    public:

        inline static int DefaultPageScrollStep=15;
        inline static int DefaultSingleScrollStep=4;

        Spinner(QWidget* parent=nullptr);

        void setStyleSample(QWidget* widget);

        void scroll(SpinnerSection* section, int delta);
        void scrollTo(SpinnerSection* section, int pos);
        void setSections(std::vector<std::shared_ptr<SpinnerSection>> sections);

        void setSelectionHeight(int val)
        {
            m_selectionHeight=val;
        }

    protected:

        void paintEvent(QPaintEvent *paint) override;

        QSize sizeHint() const override;
        void keyPressEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent *event) override;

        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void enterEvent(QEnterEvent *event) override;

    private:

        std::shared_ptr<SpinnerSection> sectionUnderCursor() const;

        QWidget* m_styleSample;
        std::vector<std::shared_ptr<SpinnerSection>> m_sections;

        int m_singleScrollStep;
        int m_pageScrollStep;

        float m_wheelOffsetAccumulated;

        QPoint m_lastMousePos;
        bool m_mousePressed;
        std::shared_ptr<SpinnerSection> m_sectionUnderCursor;
        int m_selectionHeight;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SPINNER_HPP
