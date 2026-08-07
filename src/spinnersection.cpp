/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/src/spinnersection.cpp
*
*  Defines SpinnerSection.
*
*/

/****************************************************************************/

#include <uise/desktop/spinner.hpp>
#include <uise/desktop/spinnersection.hpp>
#include <uise/desktop/detail/spinnersection_p.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

//--------------------------------------------------------------------------
SpinnerSection::SpinnerSection() : pimpl(std::make_unique<detail::SpinnerSection_p>())
{}

//--------------------------------------------------------------------------
SpinnerSection::~SpinnerSection()
{}

//--------------------------------------------------------------------------
int SpinnerSection::width() const noexcept
{
    return pimpl->itemsWidth+pimpl->leftBarWidth+pimpl->rightBarWidth;
}

//--------------------------------------------------------------------------
int SpinnerSection::itemsWidth() const noexcept
{
    return pimpl->itemsWidth;
}

//--------------------------------------------------------------------------
void SpinnerSection::setItemsWidth(int val) noexcept
{
    pimpl->itemsWidth=val;
}

//--------------------------------------------------------------------------
int SpinnerSection::leftBarWidth() const noexcept
{
    return pimpl->leftBarWidth;
}

//--------------------------------------------------------------------------
void SpinnerSection::setLeftBarWidth(int val) noexcept
{
    pimpl->leftBarWidth=val;
}

//--------------------------------------------------------------------------
int SpinnerSection::rightBarWidth() const noexcept
{
    return pimpl->rightBarWidth;
}

//--------------------------------------------------------------------------
void SpinnerSection::setRightBarWidth(int val) noexcept
{
    pimpl->rightBarWidth=val;
}

//--------------------------------------------------------------------------
bool SpinnerSection::circular() const noexcept
{
    return pimpl->circular;
}

//--------------------------------------------------------------------------
void SpinnerSection::setCircular(bool enable) noexcept
{
    pimpl->circular=enable;
}

//--------------------------------------------------------------------------
void SpinnerSection::setItems(QList<QWidget *> items)
{
    pimpl->items=std::move(items);

    // keep the enabled mask parallel to items: preserve existing flags by position, default new
    // entries to enabled. QList::resize() default-constructs new elements to false, which is
    // the wrong default here, so grow with explicit true values instead.
    if (!pimpl->itemsEnabled.isEmpty())
    {
        while (pimpl->itemsEnabled.size()<pimpl->items.size())
        {
            pimpl->itemsEnabled.append(true);
        }
        pimpl->itemsEnabled.resize(pimpl->items.size());
    }
    pimpl->updateEnabledBounds();
}

//--------------------------------------------------------------------------
void SpinnerSection::setLeftBarLabel(QWidget *widget) noexcept
{
    pimpl->leftBarLabel=widget;
}

//--------------------------------------------------------------------------
void SpinnerSection::setRightBarLabel(QWidget *widget) noexcept
{
    pimpl->rightBarLabel=widget;
}

//--------------------------------------------------------------------------
int SpinnerSection::itemsCount() const noexcept
{
    return pimpl->items.size();
}

//--------------------------------------------------------------------------
void SpinnerSection::setEnabledRange(int first, int last) noexcept
{
    auto n=pimpl->items.size();
    if (n<=0)
    {
        return;
    }

    first=qBound(0,first,n-1);
    last=qBound(first,last,n-1);

    pimpl->itemsEnabled.clear();
    for (int i=0;i<n;++i)
    {
        pimpl->itemsEnabled.append(i>=first && i<=last);
    }
    pimpl->updateEnabledBounds();
}

//--------------------------------------------------------------------------
void SpinnerSection::resetEnabledItems() noexcept
{
    pimpl->itemsEnabled.clear();
    pimpl->updateEnabledBounds();
}

//--------------------------------------------------------------------------
void SpinnerSection::setItemEnabled(int index, bool enable) noexcept
{
    auto n=pimpl->items.size();
    if (index<0 || index>=n)
    {
        return;
    }

    if (pimpl->itemsEnabled.isEmpty())
    {
        if (enable)
        {
            // already enabled -- no mask needed
            return;
        }
        pimpl->itemsEnabled.reserve(n);
        for (int i=0;i<n;++i)
        {
            pimpl->itemsEnabled.append(true);
        }
    }

    pimpl->itemsEnabled[index]=enable;
    pimpl->updateEnabledBounds();
}

//--------------------------------------------------------------------------
bool SpinnerSection::itemEnabled(int index) const noexcept
{
    if (pimpl->itemsEnabled.isEmpty())
    {
        return true;
    }
    if (index<0 || index>=pimpl->itemsEnabled.size())
    {
        return false;
    }
    return pimpl->itemsEnabled.at(index);
}

//--------------------------------------------------------------------------
bool SpinnerSection::hasDisabledItems() const noexcept
{
    return pimpl->masked;
}

//--------------------------------------------------------------------------
int SpinnerSection::firstEnabledIndex() const noexcept
{
    return pimpl->firstEnabled;
}

//--------------------------------------------------------------------------
int SpinnerSection::lastEnabledIndex() const noexcept
{
    return pimpl->lastEnabled;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
