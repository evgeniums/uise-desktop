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

/** @file uise/desktop/frame.hpp
*
*  Declares Frame.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_FRAME_HPP
#define UISE_DESKTOP_FRAME_HPP

#include <QFrame>

#include <uise/desktop/widget.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT Frame : public QFrame
{
    Q_OBJECT

    Q_PROPERTY(int uisePadding READ getUisePadding WRITE setUisePadding NOTIFY uisePaddingChanged)

    public:

        using QFrame::QFrame;

        int getUisePadding() const noexcept
        {
            return m_uisePadding;
        }

        void setUisePadding(int value) noexcept
        {
            m_uisePadding = value;
        }

    signals:

        void uisePaddingChanged(int value);

    private:

        int m_uisePadding=0;
};

class UISE_DESKTOP_EXPORT WidgetQFrame : public Frame,
                                         public Widget
{
    Q_OBJECT

    public:

        using Frame::Frame;

        QWidget* qWidget() override
        {
            return this;
        }
};

inline int horizontalTotalMargin(const QFrame* frame)
{
    QMargins margins = frame->contentsMargins();
    return margins.left()+margins.right();
}

}

#endif // UISE_DESKTOP_FRAME_HPP
