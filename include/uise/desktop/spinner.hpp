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

/**
 * @brief Spinner is a view for representing lists of items that look like combo boxes (spinners) on smartphones.
 *
 * Spinner can represent a few item lists at once.
 * Each list is processed by a spinner section representing a scrollable list of items.
 *
 * A spinner section can be expanded with left and/or right bar containing static label in the center of a section.
 *
 * A spinner section can be either linear or circular which is enabled by SpinnerSection::setCircular().
 * Different sections of the same spinner can be of different modes.
 *
 * All items in all spinner sections have the same height that must be set with setItemHeight().
 *
 */
class UISE_DESKTOP_EXPORT Spinner : public QFrame
{
    Q_OBJECT

    public:

        inline static int DefaultPageScrollStep=60;
        inline static int DefaultSingleScrollStep=4;
        inline static int WheelScrollStep=4;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        Spinner(QWidget* parent=nullptr);
        ~Spinner();

        Spinner(const Spinner&) = delete;
        Spinner(Spinner&&) = delete;
        Spinner& operator=(const Spinner&) = delete;
        Spinner& operator=(Spinner&&) = delete;

        /**
         * @brief Set sample widget to be used as style reference.
         * @param widget Reference widget.
         *
         * The spinner will own sample widget.
         */
        void setStyleSample(QWidget* widget);

        /**
         * @brief Load sections to spinner.
         * @param sections Spinner sections.
         *
         * Note that the sections must be populated before loading to spinner.
         */
        void setSections(std::vector<std::shared_ptr<SpinnerSection>> sections);

        /**
         * @brief Get spinner section by index.
         * @param index Section index.
         * @return Query result.
         */
        std::shared_ptr<SpinnerSection> section(int index) const;

        /**
         * @brief Get number of sections in spinner.
         * @return Query result.
         */
        size_t sectionCount() const noexcept;

        /**
         * @brief scroll Scroll spinner section by delta offset.
         * @param sectionIndex Section index.
         * @param delta Offset.
         */
        void scroll(int sectionIndex, int delta)
        {
            scroll(section(sectionIndex).get(),delta);
        }

        /**
         * @brief Scroll spinner section to certain position.
         * @param sectionIndex Section index.
         * @param pos Target position.
         */
        void scrollTo(int sectionIndex, int pos)
        {
            scrollTo(section(sectionIndex).get(),pos);
        }

        /**
         * @brief Get index of currently selected item in spinner section.
         * @param sectionIndex Section index.
         * @return Query result.
         */
        int selectedItemIndex(int sectionIndex) const
        {
            return selectedItemIndex(section(sectionIndex).get());
        }

        /**
         * @brief Select item in a spinner section.
         * @param sectionIndex Section index.
         * @param index Item index.
         */
        void selectItem(int sectionIndex, int index)
        {
            selectItem(section(sectionIndex).get(),index);
        }

        /**
         * @brief Set fixed height of spinner item.
         * @param val Height of spinner item in pixels.
         *
         */
        void setItemHeight(int val) noexcept;

        /**
         * @brief Get height of a spinner item.
         * @return Query result.
         */
        int itemHeight() const noexcept;

        /**
         * @brief Append items to spinner section.
         * @param section Section index.
         * @param items List of widgets to append.
         *
         * The spinner will own appended items.
         */
        void appendItems(int section, const QList<QWidget*>& items);

        /**
         * @brief Remove last items from spinner section.
         * @param section Section index.
         * @param count Number of items to remove.
         */
        void removeLastItems(int section, int count);

        /**
         * @brief Append item to spinner section.
         * @param section Section index.
         * @param item Widget to append.
         *
         * The spinner will own appended item.
         */
        void appendItem(int section, QWidget* item)
        {
            QList<QWidget*> items{item};
            appendItems(section,items);
        }

        /**
         * @brief Remove last item from spinner section.
         * @param section Section index.
         */
        void removeLastItem(int section)
        {
            removeLastItems(section,1);
        }

        QSize sizeHint() const override;

    signals:

        /**
         * @brief Notify that item selection was changed in a section.
         * @param sectionIndex Section index.
         * @param itemIndex New item index.
         */
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
