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
#include <uise/desktop/utils/wheeleventhandler.hpp>

#include <uise/desktop/spinnersection.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class Spinner_p;

/**
 * @brief Spinner is a view for representing lists of items that look like combo boxes (spinners) on smartphones.
 *
 * Spinner can represent a few item lists at once.
 * Each list is processed by a spinner section representing a scrollable list of items.
 *
 * A spinner section can be extended with left and/or right bar containing static label in the center of a section.
 *
 * A spinner section can be either linear or circular which is enabled by SpinnerSection::setCircular().
 * Different sections of the same spinner can be of different modes.
 *
 * All items in all spinner sections have the same height that must be set with setItemHeight().
 *
 */
class UISE_DESKTOP_EXPORT Spinner : public QFrame,
                                    public WheelEventHandler
{
    Q_OBJECT

    public:

        inline static int DefaultPageScrollStep=60;
        inline static int DefaultSingleScrollStep=4;
        inline static int WheelScrollStep=4;
        inline static int ClickScrollDurationMs=200;

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

        /**
         * @brief Enable a contiguous range of items in a spinner section, disabling every other
         *  item.
         * @param sectionIndex Section index.
         * @param first Index of the first enabled item.
         * @param last Index of the last enabled item.
         *
         * Unlike SpinnerSection::setEnabledRange(), this also re-clamps the current scroll
         * position into the new range if it fell outside -- see clampOffset()/
         * enforceEnabledItems(). Scrolling (wheel/drag/keys/click) is then hard-clamped to the
         * range's outer bounds: for a circular section this makes the wheel stop at the
         * boundary instead of wrapping past it.
         */
        void setEnabledRange(int sectionIndex, int first, int last)
        {
            setEnabledRange(section(sectionIndex).get(),first,last);
        }

        /**
         * @brief Enable every item in a spinner section, clearing any previously set mask.
         * @param sectionIndex Section index.
         */
        void resetEnabledItems(int sectionIndex)
        {
            resetEnabledItems(section(sectionIndex).get());
        }

        /**
         * @brief Enable or disable a single item in a spinner section.
         * @param sectionIndex Section index.
         * @param index Item index.
         * @param enable Whether the item should be enabled.
         */
        void setItemEnabled(int sectionIndex, int index, bool enable)
        {
            setItemEnabled(section(sectionIndex).get(),index,enable);
        }

        /**
         * @brief Check if an item is enabled.
         * @param sectionIndex Section index.
         * @param index Item index.
         * @return Query result.
         */
        bool itemEnabled(int sectionIndex, int index) const
        {
            return section(sectionIndex)->itemEnabled(index);
        }

        /**
         * @brief Get index of the first enabled item in a spinner section.
         * @param sectionIndex Section index.
         * @return Query result.
         */
        int firstEnabledIndex(int sectionIndex) const
        {
            return section(sectionIndex)->firstEnabledIndex();
        }

        /**
         * @brief Get index of the last enabled item in a spinner section.
         * @param sectionIndex Section index.
         * @return Query result.
         */
        int lastEnabledIndex(int sectionIndex) const
        {
            return section(sectionIndex)->lastEnabledIndex();
        }

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

        std::tuple<int,int,int,int> calcTopItem(SpinnerSection *section) const;
        void updateCurrentIndex(SpinnerSection* section);

        void scroll(SpinnerSection* section, int delta);
        void scrollTo(SpinnerSection* section, int pos);
        int selectedItemIndex(SpinnerSection* section) const;
        void selectItem(SpinnerSection* section, int index);

        /** @brief Clamp a candidate offset into range for a non-circular section, or for a
         *  masked circular one (a no-op for an unmasked circular section, which wraps freely).
         *  Shared by scrollTo() and the click-to-scroll animation.
         *
         *  Deliberately does NO modular arithmetic: every caller derives pos from currentOffset
         *  by plain arithmetic in the same unwrapped frame, and enforceEnabledItems() guarantees
         *  currentOffset is already inside [lo,hi]. Folding pos back by whole periods here would
         *  let one large step (PageDown, a fast wheel/drag) cross the midpoint of the forbidden
         *  gap and snap to the OPPOSITE boundary -- i.e. wrap, exactly what masking must prevent. */
        int clampOffset(SpinnerSection* section, int pos) const;

        /** @brief Offset at which item `index` sits exactly in the selection band. */
        int offsetForIndex(SpinnerSection* section, int index) const;

        /** @brief Index of the first enabled item (0 when the section has no mask). */
        int firstEnabledIndex(SpinnerSection* section) const;

        /** @brief Index of the last enabled item (items.size()-1 when the section has no mask). */
        int lastEnabledIndex(SpinnerSection* section) const;

        /** @brief Clamp an item index into the section's enabled range. */
        int clampIndex(SpinnerSection* section, int index) const;

        /** @brief Re-establish the currentOffset-in-range invariant after the enabled range or
         *  the item count changed. The only place that folds a circular section's currentOffset
         *  by whole periods (a visual no-op, see calcTopItem()) -- everywhere else clampOffset()
         *  assumes that invariant already holds. */
        void enforceEnabledItems(SpinnerSection* section);

        /** @brief Set/clear the "itemDisabled" dynamic property (drives QSS greying) on every
         *  item widget in a section, according to its current mask. */
        void updateItemsDisabledState(SpinnerSection* section);

        void setEnabledRange(SpinnerSection* section, int first, int last);
        void resetEnabledItems(SpinnerSection* section);
        void setItemEnabled(SpinnerSection* section, int index, bool enable);

        /** @brief Animate currentOffset from wherever it is now to targetOffset, in response to
         *  a click on a visible item -- see mousePressEvent()/mouseReleaseEvent(). Deliberately
         *  does NOT call updateCurrentIndex() while in flight (see paintEvent()/calcTopItem()),
         *  so only one itemChanged() fires, at the end. */
        void animateScrollTo(SpinnerSection* section, int targetOffset);

        QRect selectionRect() const;
        QRect selectionRect(int height, int offset) const;
        QRect selectionRect(SpinnerSection* section) const;

        std::shared_ptr<SpinnerSection> sectionUnderCursor() const;
        void adjustPosition(SpinnerSection* section, bool animate=true, bool noDelay=false);

        int itemsHeight(SpinnerSection* section) const;
        int sectionHeight(SpinnerSection* section) const;
        int sectionOffset(SpinnerSection* section) const;

        void notifySelectionChanged(SpinnerSection* section);

        std::unique_ptr<Spinner_p> pimpl;
};

}

#endif // UISE_DESKTOP_SPINNER_HPP
