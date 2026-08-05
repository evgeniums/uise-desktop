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

/** @file uise/desktop/abstractdialog.hpp
*
*  Declares AbstractDialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_ABSTRACT_DIALOG_HPP
#define UISE_DESKTOP_ABSTRACT_DIALOG_HPP

#include <optional>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/frame.hpp>
#include <uise/desktop/style.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;

class UISE_DESKTOP_EXPORT AbstractDialog : public WidgetQFrame
{
    Q_OBJECT

    //! QSS: qproperty-buttonsOrientation: "vertical";
    Q_PROPERTY(QString buttonsOrientation READ buttonsOrientationName WRITE setButtonsOrientationName)
    //! QSS: qproperty-buttonsAlignment: "right bottom";  -- ALWAYS quote a multi-token
    //! value, Qt's CSS tokenizer otherwise splits it and only the first token survives.
    Q_PROPERTY(QString buttonsAlignment READ buttonsAlignmentName WRITE setButtonsAlignmentName)

    public:

        enum class StandardButton : int
        {
            Close=0,
            Accept=1,
            OK=2,
            Apply=3,
            Yes=4,
            Ignore=5,
            Cancel=6,
            No=7,
            Skip=8,
            Retry=9,
            Next=10,
            Back=11,
            Start=12,
            Finish=13,
            Complete=14,
            Done=15
        };

        struct ButtonConfig
        {
            int id=0;
            QString text;
            std::shared_ptr<SvgIcon> icon;
            QString name;

            ButtonConfig(int id, QString text, std::shared_ptr<SvgIcon> icon={})
                : id(id),
                  text(std::move(text)),
                  icon(std::move(icon))
            {
                name=this->text;
            }

            ButtonConfig(StandardButton button, QWidget* parent=nullptr) : ButtonConfig(standardButton(button,parent))
            {}
        };

        using WidgetQFrame::WidgetQFrame;

        virtual void setButtons(std::vector<ButtonConfig> buttons)=0;

        static ButtonConfig standardButton(StandardButton button, QWidget* parent=nullptr);

        static bool isButton(int id, StandardButton button)
        {
            return id==static_cast<int>(button);
        }

        template <typename T>
        void setButtonVisible(T id, bool enable)
        {
            setButtonVisible(static_cast<int>(id),enable);
        }

        template <typename T>
        void setButtonEnabled(T id, bool enable)
        {
            setButtonEnabled(static_cast<int>(id),enable);
        }

        template <typename T>
        void activateButton(T id)
        {
            activateButton(static_cast<int>(id));
        }

        virtual void setTitle(const QString& title)=0;

        virtual void setDialogFocus()
        {}

        /**
         * @brief Widget a host can drag the dialog by, e.g. FloatingDialogFrame.
         * @return The dialog's title bar, or nullptr if this implementation has none.
         */
        virtual QWidget* titleBar() const
        {
            return nullptr;
        }

        virtual void setClosable(bool enable)
        {
            std::ignore=enable;
        }

        /**
         * @brief Override the whole ButtonsStyle for this dialog.
         *
         * Layers over the global Style::buttonsStyle(buttonsStyleContext(), this) but
         * BELOW the per-axis overrides set by setButtonsOrientation()/setButtonsAlignment()
         * and by the buttonsOrientation/buttonsAlignment QSS properties.
         *
         * Schedules a deferred relayout of the already-created buttons, so unlike before
         * it no longer needs to be called before setButtons(). Note that showText/showIcon
         * still only affect buttons created AFTER this call -- they are consumed by
         * standardButton() when a ButtonConfig is built, not at layout time.
         */
        virtual void setButtonsStyle(ButtonsStyle style);
        virtual void resetButtonsStyle();

        //! Effective style: global default -> setButtonsStyle() -> per-axis overrides.
        ButtonsStyle effectiveButtonsStyle() const;

        //! Per-axis override of ButtonsStyle::orientation.
        void setButtonsOrientation(Qt::Orientation orientation);
        void resetButtonsOrientation();
        //! Effective (resolved) orientation, not the raw override.
        Qt::Orientation buttonsOrientation() const;

        //! Per-axis override of ButtonsStyle::alignment.
        void setButtonsAlignment(Qt::Alignment alignment);
        void resetButtonsAlignment();
        //! Effective (resolved) alignment, not the raw override.
        Qt::Alignment buttonsAlignment() const;

        //! QSS-friendly string form: "horizontal" | "vertical"; "" / "default" resets.
        void setButtonsOrientationName(const QString& name);
        QString buttonsOrientationName() const;

        //! QSS-friendly string form, see alignmentFromString(); "" / "default" resets.
        void setButtonsAlignmentName(const QString& name);
        QString buttonsAlignmentName() const;

    signals:

        void buttonClicked(int id);

        void closeRequested();

    public slots:

        void activateButton(int id);
        void setButtonEnabled(int id, bool enable);
        void setButtonVisible(int id, bool enable);
        void closeDialog();

    protected:

        virtual void doActivateButton(int id)=0;
        virtual void doSetButtonEnabled(int id, bool enable)=0;
        virtual void doSetButtonVisible(int id, bool enable)=0;

        /**
         * @brief Re-apply the effective ButtonsStyle to the already-created buttons.
         *
         * Implementations must NOT destroy/recreate the buttons: callers set per-button
         * enabled/visible state after setButtons() (see
         * whitemdesktop/ui/uiapplock.cpp, which hides a button right after setButtons()),
         * and that state would be lost. No-op in the base class.
         */
        virtual void updateButtonsLayout()
        {}

        /**
         * @brief Request updateButtonsLayout() on the next event loop turn, coalescing
         *        multiple requests into one.
         *
         * Deferral is mandatory, not an optimisation: setButtonsOrientationName()/
         * setButtonsAlignmentName() are Q_PROPERTY writers and Qt's style engine calls them
         * DURING polish. Relayouting + repolishing this same widget tree synchronously from
         * inside that call re-enters the style sheet engine on a widget it is still
         * polishing, which crashes -- see the identical guard and its comment in
         * FileUploadWidget::deferListAreaHeightUpdate(), src/fileuploadwidget.cpp.
         */
        void scheduleButtonsLayoutUpdate();

        //! Context name passed to Style::buttonsStyle(). "Dialog" by default.
        virtual QString buttonsStyleContext() const
        {
            return QStringLiteral("Dialog");
        }

    private:

        std::optional<ButtonsStyle> m_forceButtonsStyle;
        std::optional<Qt::Orientation> m_buttonsOrientation;
        std::optional<Qt::Alignment> m_buttonsAlignment;
        bool m_buttonsLayoutUpdateScheduled=false;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_ABSTRACT_DIALOG_HPP
