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

/** @file uise/desktop/spinnersection.hpp
*
*  Declares SpinnerSection.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SPINNERSECTION_HPP
#define UISE_DESKTOP_SPINNERSECTION_HPP

#include <memory>
#include <QList>

class QWidget;

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class Spinner;

namespace detail
{
class SpinnerSection_p;
};

/**
 * @brief Section of a Spinner.
 */
class UISE_DESKTOP_EXPORT SpinnerSection
{
    public:

        /**
         * @brief Constructor.
         */
        SpinnerSection();

        SpinnerSection(const SpinnerSection&) = delete;
        SpinnerSection(SpinnerSection&&) = delete;
        SpinnerSection& operator=(const SpinnerSection&) = delete;
        SpinnerSection& operator=(SpinnerSection&&) = delete;

        /**
         * @brief Destructor.
         */
        ~SpinnerSection();

        /**
         * @brief Full width of the section.
         * @return Query result.
         */
        int width() const noexcept;

        /**
         * @brief Load list of items to section.
         * @param items List of widgets.
         *
         * Spinner will own the widgets after loading this section to the spinner.
         */
        void setItems(QList<QWidget*> items);

        /**
         * @brief Set widget to be used as a selection label in the left bar.
         * @param widget Bar label
         *
         * Spinner will own the label after loading this section to the spinner.
         */
        void setLeftBarLabel(QWidget* widget=nullptr) noexcept;

        /**
         * @brief Set widget to be used as a selection label in the right bar.
         * @param widget Bar label.
         *
         * Spinner will own the label after loading this section to the spinner.
         */
        void setRightBarLabel(QWidget* widget=nullptr) noexcept;

        /**
         * @brief Set width of items list.
         * @param val Width in pixels.
         */
        void setItemsWidth(int val) noexcept;

        /**
         * @brief Get width of items list.
         * @return Width in pixels.
         */
        int itemsWidth() const noexcept;

        /**
         * @brief Get number of items in the section.
         * @return Query result.
         */
        int itemsCount() const noexcept;

        /**
         * @brief Set width of left bar.
         * @param val Width in pixels.
         */
        void setLeftBarWidth(int val) noexcept;

        /**
         * @brief Get width of left bar.
         * @return Width in pixels.
         */
        int leftBarWidth() const noexcept;

        /**
         * @brief Set width of right bar.
         * @param val Width in pixels.
         */
        void setRightBarWidth(int val) noexcept;

        /**
         * @brief Get width of right bar.
         * @return Width in pixels.
         */
        int rightBarWidth() const noexcept;

        /**
         * @brief Set circular mode of the section.
         * @param enable Mode.
         */
        void setCircular(bool enable) noexcept;

        /**
         * @brief Check if this section is in circular mode.
         * @return True if section is in circular mode.
         */
        bool circular() const noexcept;

        /**
         * @brief Enable a contiguous range of items, disabling every other item.
         * @param first Index of the first enabled item (clamped to [0,itemsCount()-1]).
         * @param last Index of the last enabled item (clamped to [first,itemsCount()-1]).
         *
         * Spinner enforces the mask by clamping scroll position to the range's outer bounds --
         * see Spinner::setEnabledRange(). Items disabled here that sit strictly between the
         * first and last enabled item stay reachable: the clamp can only express a contiguous
         * allowed span, so an interior gap has no effect on scrolling, only on itemEnabled().
         *
         * NOTE: mutating this on a section already loaded into a Spinner does not by itself
         * re-clamp the current scroll position -- use the Spinner overload of setEnabledRange()
         * for that.
         */
        void setEnabledRange(int first, int last) noexcept;

        /**
         * @brief Enable every item, clearing any mask set by setEnabledRange()/setItemEnabled().
         *
         * NOTE: see setEnabledRange() for the same caveat about a section already loaded into a
         * Spinner -- use Spinner::resetEnabledItems() there instead.
         */
        void resetEnabledItems() noexcept;

        /**
         * @brief Enable or disable a single item.
         * @param index Item index.
         * @param enable Whether the item should be enabled.
         *
         * NOTE: see setEnabledRange() for the same caveat about a section already loaded into a
         * Spinner -- use Spinner::setItemEnabled() there instead.
         */
        void setItemEnabled(int index, bool enable) noexcept;

        /**
         * @brief Check if an item is enabled.
         * @param index Item index.
         * @return True if the item is enabled (the default, when no mask is set).
         */
        bool itemEnabled(int index) const noexcept;

        /**
         * @brief Check if this section has any disabled item.
         * @return Query result.
         */
        bool hasDisabledItems() const noexcept;

        /**
         * @brief Get index of the first enabled item.
         * @return Query result. 0 when there is no mask.
         */
        int firstEnabledIndex() const noexcept;

        /**
         * @brief Get index of the last enabled item.
         * @return Query result. itemsCount()-1 when there is no mask.
         */
        int lastEnabledIndex() const noexcept;

    private:

        std::unique_ptr<detail::SpinnerSection_p> pimpl;

        friend class Spinner;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SPINNERSECTION_HPP
