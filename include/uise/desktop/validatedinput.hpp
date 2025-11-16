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

/** @file uise/desktop/validatedinput.hpp
*
*  Declares ValidatedInput.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_VALIDATED_INPUT_HPP
#define UISE_DESKTOP_VALIDATED_INPUT_HPP

#include <QLineEdit>
#include <QValidator>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/widget.hpp>

class QLabel;

UISE_DESKTOP_NAMESPACE_BEGIN

class PushButton;

class UISE_DESKTOP_EXPORT AbstractValidatedInput : public WidgetQFrame
{
    Q_OBJECT

    public:

        using WidgetQFrame::WidgetQFrame;

        virtual void clear() =0;

        virtual void setText(const QString& text) =0;
        virtual std::optional<QString> text() const =0;

        virtual void setApplyButtonText(const QString& text) =0;
        virtual QString applyButtonText() const =0;

        virtual void setApplyButtonVisible(bool enable) =0;
        virtual bool isApplyButtonVisible() const =0;

        virtual void setPlaceholderText(const QString& text) =0;
        virtual QString placeholderText() const =0;

        virtual QWidget* editorWidget() const =0;

        void setValidator(std::shared_ptr<QValidator> validator)
        {
            m_validator=validator;
            updateValidator();
        }

        QValidator* validator() const noexcept
        {
            return m_validator.get();
        }

        std::shared_ptr<QValidator> sharedValidator() const noexcept
        {
            return m_validator;
        }

        virtual void showError(const QString& msg) =0;

    signals:

        void applyRequested();

    protected:

        virtual void updateValidator() =0;

    private:

        std::shared_ptr<QValidator> m_validator;
};

class ValidatedInput_p;

class UISE_DESKTOP_EXPORT ValidatedInput : public AbstractValidatedInput
{
    Q_OBJECT

    public:

        ValidatedInput(QWidget* parent=nullptr);

        virtual void clear() override;

        virtual void setText(const QString& text) override;
        virtual std::optional<QString> text() const override;

        virtual void setApplyButtonText(const QString& text) override;
        virtual QString applyButtonText() const override;

        virtual void setApplyButtonVisible(bool enable) override;
        virtual bool isApplyButtonVisible() const override;

        virtual void setPlaceholderText(const QString& text) override;
        virtual QString placeholderText() const override;

        virtual QWidget* editorWidget() const override;

        void showError(const QString& msg) override;

    private slots:

        void onTextChanged(QString text);
        void tryApply();

    protected:

        void updateValidator() override;

    private:

        QLineEdit* m_input;
        PushButton* m_button;
        QLabel* m_error;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_VALIDATED_INPUT_HPP
