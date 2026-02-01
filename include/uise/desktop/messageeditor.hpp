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

/** @file uise/desktop/messageeditor.hpp
*
*  Declares MessageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MESSAGEEDITOR_HPP
#define UISE_DESKTOP_MESSAGEEDITOR_HPP

#include <QTextEdit>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractmessageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class UISE_DESKTOP_EXPORT EnhancedTextEdit : public QTextEdit
{
    Q_OBJECT

    public:

        explicit EnhancedTextEdit(QWidget *parent = nullptr);

        QSize sizeHint() const override;

        void setAutoResizingEnabled(bool enable);
        bool isAutoResizingEnabled() const noexcept
        {
            return m_autoResize;
        }

        void setNewLineOnEnter(bool enable)
        {
            m_newLineOnEnter=enable;
        }

        bool isNewLineOnEnter() const noexcept
        {
            return m_newLineOnEnter;
        }

    signals:

        void returnPressed();

    protected:

        void keyPressEvent(QKeyEvent* event) override;

    private slots:

        void updateSize();

    private:

        bool m_autoResize;
        bool m_newLineOnEnter;
};

class MessageEditor_p;

class UISE_DESKTOP_EXPORT MessageEditor : public AbstractMessageEditor
{
    Q_OBJECT

    public:

        explicit MessageEditor(QWidget* parent=nullptr);

        ~MessageEditor();

        MessageEditor(const MessageEditor&) =delete;
        MessageEditor& operator=(const MessageEditor&) =delete;
        MessageEditor(MessageEditor&&) =delete;
        MessageEditor& operator=(MessageEditor&&) =delete;

        void loadText(const QString& text, TextFormat format=TextFormat::Markdown) override;

        QString text(TextFormat format=TextFormat::Markdown) const override;

        QString selectedText(TextFormat format=TextFormat::Markdown) const override;

        void setFocusIn() override;

        void setPlaceHolderText(const QString& text) override;

    public slots:

        void selectAll() override;

        void clearSelection() override;

        void clear() override;

    protected:

        void updateEditingMode() override;
        virtual void updateFinishOnEnter() override;
        virtual void updateEditingFinished() override;

    private:

        void setupReturnPressed();

        std::unique_ptr<MessageEditor_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MESSAGEEDITOR_HPP
