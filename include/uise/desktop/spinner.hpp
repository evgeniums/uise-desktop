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
#include <uise/desktop/utils/singleshottimer.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SpinnerSection;
class Spinner_p;

class UISE_DESKTOP_EXPORT Spinner : public QFrame
{
    Q_OBJECT

    public:

        inline static int DefaultPageScrollStep=60;
        inline static int DefaultSingleScrollStep=4;
        inline static int WheelScrollStep=4;

        Spinner(QWidget* parent=nullptr);
        ~Spinner();

        Spinner(const Spinner&) = delete;
        Spinner(Spinner&&) = delete;
        Spinner& operator=(const Spinner&) = delete;
        Spinner& operator=(Spinner&&) = delete;

        void setStyleSample(QWidget* widget);

        void setSections(std::vector<std::shared_ptr<SpinnerSection>> sections);
        std::shared_ptr<SpinnerSection> section(int index) const;
        size_t sectionCount() const noexcept;

        void scroll(int sectionIndex, int delta)
        {
            scroll(section(sectionIndex).get(),delta);
        }
        void scrollTo(int sectionIndex, int pos)
        {
            scrollTo(section(sectionIndex).get(),pos);
        }

        int selectedItemIndex(int sectionIndex) const
        {
            return selectedItemIndex(section(sectionIndex).get());
        }
        void selectItem(int sectionIndex, int index)
        {
            selectItem(section(sectionIndex).get(),index);
        }

        void setItemHeight(int val) noexcept;
        int itemHeight() const noexcept;

        void appendItems(int section, const QList<QWidget*>& items);
        void removeLastItems(int section, int count);

        void appendItem(int section, QWidget* item)
        {
            QList<QWidget*> items{item};
            appendItems(section,items);
        }
        void removeLastItem(int section)
        {
            removeLastItems(section,1);
        }

        QSize sizeHint() const override;

    signals:

        void itemChanged(int sectionIndex, int itemIndex);

    protected:

        void paintEvent(QPaintEvent *paint) override;

        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent *event) override;

        void mouseMoveEvent(QMouseEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void enterEvent(QEnterEvent *event) override;

    private:

        void scroll(SpinnerSection* section, int delta);
        void scrollTo(SpinnerSection* section, int pos);
        int selectedItemIndex(SpinnerSection* section) const;
        void selectItem(SpinnerSection* section, int index);

        QRect selectionRect() const;
        QRect selectionRect(int height, int offset) const;
        QRect selectionRect(SpinnerSection* section) const;

        std::shared_ptr<SpinnerSection> sectionUnderCursor() const;
        void adjustPosition(SpinnerSection* section, bool animate=true, bool noDelay=false);

        int itemsHeight(SpinnerSection* section) const;
        int sectionHeight(SpinnerSection* section) const;
        int sectionOffset(SpinnerSection* section) const;

        std::unique_ptr<Spinner_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SPINNER_HPP
