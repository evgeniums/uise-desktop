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

/** @file uise/desktop/elidedlabel.hpp
*
*  Declares ElidedLabel
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ELIDED_LABEL_HPP
#define UISE_DESKTOP_ELIDED_LABEL_HPP

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

class QLabel;

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class UISE_DESKTOP_EXPORT ElidedLabel : public QFrame
{
    Q_OBJECT

    public:

        explicit ElidedLabel(QWidget *parent = 0) : ElidedLabel("",parent)
        {}

        explicit ElidedLabel(const QString &text, QWidget *parent = 0);

        void setText(const QString &text);
        QString text() const;

        void setElideMode(Qt::TextElideMode elideMode) noexcept
        {
            m_mode=elideMode;
        }

        Qt::TextElideMode elideMode() const noexcept
        {
            return m_mode;
        }

        QSize sizeHint() const override;

        void setAlignment(Qt::Alignment alignment);
        Qt::Alignment alignment() const;

        void setIgnoreSizeHint(bool enable);

        bool isIgnoreSizeHint() const noexcept
        {
            return m_ignoreSizeHint;
        }

        void setMaxLines(int count);
        int maxLines() const noexcept
        {
            return m_maxLines;
        }

        int widthHint() const;

    signals:

        void textUpdated();

    protected:

        void resizeEvent(QResizeEvent *event) override;
        void showEvent(QShowEvent *event) override;

    private:

        void updateText(int width);

        QString m_content;
        Qt::TextElideMode m_mode;
        QLabel* m_label;
        QLabel* m_hiddenLabel;

        bool m_ignoreSizeHint;
        int m_maxLines;

        // Guard the FIRST elide against Qt's default pre-layout width (see setText()'s own
        // comment) -- a label built off-screen (e.g. a chat message widget, negotiated before
        // it is ever shown) would otherwise elide against width()==100 and paint a truncated
        // first frame that only self-corrects a turn later, once a queued LayoutRequest resizes
        // it. m_laidOut latches true forever once a real width has been applied (by resizeEvent()
        // or, as a fallback, showEvent()); m_elidePending tracks whether setText() was called
        // while still waiting for that first real width.
        bool m_laidOut=false;
        bool m_elidePending=false;
};

}

#endif // UISE_DESKTOP_ELIDED_LABEL_HPP
