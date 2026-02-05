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

/** @file uise/desktop/abstractmessageeditor.hpp
*
*  Declares AbstractMessageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACTMESSAGEEDITOR_HPP
#define UISE_DESKTOP_ABSTRACTMESSAGEEDITOR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

enum class TextFormat : int
{
    Plain,
    Markdown,
    Html
};

class UISE_DESKTOP_EXPORT AbstractMessageEditor : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void loadText(const QString& text, TextFormat format=TextFormat::Markdown) =0;

        virtual QString text(TextFormat format=TextFormat::Markdown) const =0;

        virtual QString selectedText(TextFormat format=TextFormat::Markdown) const =0;

        void setEditingMode(Qt::TextFormat mode)
        {
            m_editingMode=mode;
            updateEditingMode();
        }

        Qt::TextFormat editingMode() const noexcept
        {
            return m_editingMode;
        }

        void setFinishOnEnter(bool enable)
        {
            m_finishOnEnter=enable;
            updateFinishOnEnter();
        }

        bool isFinishOnEnter() const noexcept
        {
            return m_finishOnEnter;
        }

        virtual void setFocusIn() =0;

        virtual void setPlaceHolderText(const QString& text) =0;

    public slots:

        void finishEditing()
        {
            updateEditingFinished();
            emit editingFinished();
        }

        virtual void selectAll() =0;

        virtual void clearSelection() =0;

        virtual void clear() =0;

    signals:

        void textChanged();
        void editingFinished();
        void activated();

    protected:

        virtual void updateEditingMode() {}
        virtual void updateFinishOnEnter() {}
        virtual void updateEditingFinished() {}

    private:

        Qt::TextFormat m_editingMode=Qt::RichText;
        bool m_finishOnEnter=true;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACTMESSAGEEDITOR_HPP
