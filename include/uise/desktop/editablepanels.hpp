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

/** @file uise/desktop/editablepanels.hpp
*
*  Declares EditablePanels.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_EDITABLEPANELS_HPP
#define UISE_DESKTOP_EDITABLEPANELS_HPP

#include <QFrame>
#include <QBoxLayout>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT AbstractEditablePanels : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void addPanel(QWidget* panel, int stretch=0, Qt::Alignment alignment={})=0;
};

class UISE_DESKTOP_EXPORT EditablePanels : public AbstractEditablePanels
{
    Q_OBJECT

    public:

        EditablePanels(QWidget* parent=nullptr);

        void addPanel(QWidget* panel, int stretch=0, Qt::Alignment alignment={}) override;

    private:

        QBoxLayout* m_layout;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_EDITABLEPANELS_HPP
