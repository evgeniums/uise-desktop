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

/** @file uise/desktop/dialog.hpp
*
*  Declares Dialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DIALOG_HPP
#define UISE_DESKTOP_DIALOG_HPP

#include <memory>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractdialog.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SvgIcon;

template <typename BaseT>
class Dialog_p;

template <typename BaseT=AbstractDialog>
class Dialog : public BaseT
{
    public:

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        Dialog(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~Dialog();

        Dialog(const Dialog&)=delete;
        Dialog(Dialog&&)=delete;
        Dialog& operator=(const Dialog&)=delete;
        Dialog& operator=(Dialog&&)=delete;

        virtual void setButtons(std::vector<AbstractDialog::ButtonConfig> buttons) override;
        virtual void setWidget(QWidget* widget);

        void setTitle(const QString& title);

    protected:

        void doActivateButton(int id) override;
        void doSetButtonEnabled(int id, bool enable) override;
        void doSetButtons(std::vector<AbstractDialog::ButtonConfig> buttons);

    private:

        std::unique_ptr<Dialog_p<BaseT>> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_DIALOG_HPP
