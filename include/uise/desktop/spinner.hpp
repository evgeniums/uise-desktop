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
#include <QVariantAnimation>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/utils/singleshottimer.hpp>

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
    int currentItemIndex=-1;
    int currentItemPosition=-1;
    bool circular=false;
    SingleShotTimer *adjustTimer=nullptr;
    QVariantAnimation *animation=nullptr;
    SingleShotTimer *selectionTimer=nullptr;
    int animationVal=0;

    int width() const noexcept
    {
        return itemsWidth+barWidth;
    }
};

class UISE_DESKTOP_EXPORT Spinner : public QFrame
{
    Q_OBJECT

    public:

        inline static int DefaultPageScrollStep=60;
        inline static int DefaultSingleScrollStep=4;
        inline static int WheelScrollStep=4;

        Spinner(QWidget* parent=nullptr);

        void setStyleSample(QWidget* widget);

        void scroll(SpinnerSection* section, int delta);
        void scrollTo(SpinnerSection* section, int pos);

        void setSections(std::vector<std::shared_ptr<SpinnerSection>> sections);

        void setItemHeight(int val) noexcept
        {
            m_itemHeight=val;
            m_selectionHeight=val;
            m_singleScrollStep=val;
            m_pageScrollStep=5*val;
        }
        int itemHeight() const noexcept
        {
            return m_itemHeight;
        }

        QRect selectionRect() const;

        int selectedItemIndex(SpinnerSection* section) const;
        void selectItem(SpinnerSection* section, int index);

    protected:

        void paintEvent(QPaintEvent *paint) override;

        QSize sizeHint() const override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent *event) override;

        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void enterEvent(QEnterEvent *event) override;

    private:

        std::shared_ptr<SpinnerSection> sectionUnderCursor() const;
        void adjustPosition(SpinnerSection* section, bool animate=true, bool noDelay=false);

        QWidget* m_styleSample;
        std::vector<std::shared_ptr<SpinnerSection>> m_sections;

        int m_singleScrollStep;
        int m_pageScrollStep;

        float m_wheelOffsetAccumulated;

        QPoint m_lastMousePos;
        bool m_mousePressed;
        bool m_keyPressed;
        std::shared_ptr<SpinnerSection> m_sectionUnderCursor;
        int m_selectionHeight;
        int m_itemHeight;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SPINNER_HPP
