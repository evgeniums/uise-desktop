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

UISE_DESKTOP_NAMESPACE_BEGIN

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

    protected:

        void resizeEvent(QResizeEvent *event) override;

    private:

        void updateText(int width);

        QString m_content;
        Qt::TextElideMode m_mode;
        QLabel* m_label;
        QLabel* m_hiddenLabel;

        bool m_ignoreSizeHint;
        int m_maxLines;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ELIDED_LABEL_HPP
