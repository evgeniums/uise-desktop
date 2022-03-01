/**
@copyright Evgeny Sidorov 2021

This software is dual-licensed. Choose the appropriate license for your project.

1. For open source projects this software is distrubuted under the GNU GENERAL PUBLIC LICENSE, Version 3.0.
(See accompanying file LICENSE-GPLv3.md or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. For proprietary license contact evgeniums@dracosha.com.

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
         * @return
         */
        int width() const noexcept;

        /**
         * @brief Load list of items to section.
         * @param items List of widgets.
         */
        void setItems(QList<QWidget*> items);

        /**
         * @brief Set widget to be used as a selection label in the left bar.
         * @param widget Bar label
         */
        void setLeftBarLabel(QWidget* widget=nullptr) noexcept;

        /**
         * @brief Set widget to be used as a selection label in the right bar.
         * @param widget Bar label
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

    private:

        std::unique_ptr<detail::SpinnerSection_p> pimpl;

        friend class Spinner;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SPINNERSECTION_HPP
