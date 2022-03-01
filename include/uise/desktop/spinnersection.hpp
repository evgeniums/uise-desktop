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

class UISE_DESKTOP_EXPORT SpinnerSection
{
    public:

        SpinnerSection();

        SpinnerSection(const SpinnerSection&) = delete;
        SpinnerSection(SpinnerSection&&) = delete;
        SpinnerSection& operator=(const SpinnerSection&) = delete;
        SpinnerSection& operator=(SpinnerSection&&) = delete;

        ~SpinnerSection();

        int width() const noexcept;

        void setItems(QList<QWidget*> items);
        void setLeftBarLabel(QWidget* widget=nullptr) noexcept;
        void setRightBarLabel(QWidget* widget=nullptr) noexcept;

        void setItemsWidth(int val) noexcept;
        int itemsWidth() const noexcept;
        int itemsCount() const noexcept;

        void setLeftBarWidth(int val) noexcept;
        int leftBarWidth() const noexcept;

        void setRightBarWidth(int val) noexcept;
        int rightBarWidth() const noexcept;

        void setCircular(bool enable) noexcept;
        bool circular() const noexcept;

    private:

        std::unique_ptr<detail::SpinnerSection_p> pimpl;

        friend class Spinner;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SPINNERSECTION_HPP
