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
}

//--------------------------------------------------------------------------
void SpinnerSection::setLeftBarLabel(QWidget *widget) noexcept
{
    pimpl->leftBarLabel=widget;
    pimpl->leftBarLabel->setParent(pimpl->spinner);
}

//--------------------------------------------------------------------------
void SpinnerSection::setRightBarLabel(QWidget *widget) noexcept
{
    pimpl->rightBarLabel=widget;
    pimpl->rightBarLabel->setParent(pimpl->spinner);
}

//--------------------------------------------------------------------------
int SpinnerSection::itemsCount() const noexcept
{
    return pimpl->items.size();
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
