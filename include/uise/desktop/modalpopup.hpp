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

/** @file uise/desktop/modalpopup.hpp
*
*  Declares widget with modal popup dialog.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_MODAL_POPUP_HPP
#define UISE_DESKTOP_MODAL_POPUP_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class ModalPopup_p;

class FrameWithModalPopup;

class UISE_DESKTOP_EXPORT ModalPopup : public QFrame
{
    Q_OBJECT

    public:

        /**
         * @brief Constructor.
         * @param parent Parent frame whith popup widget.
         */
        ModalPopup(FrameWithModalPopup* parent=nullptr);

        ~ModalPopup();

        ModalPopup(const ModalPopup&)=delete;
        ModalPopup(ModalPopup&&)=delete;
        ModalPopup& operator=(const ModalPopup&)=delete;
        ModalPopup& operator=(ModalPopup&&)=delete;

        void setWidget(QWidget* widget);

        void close();

    protected:

        void resizeEvent(QResizeEvent *event) override;

    private:

        void updateWidgetGeometry();

        std::unique_ptr<ModalPopup_p> pimpl;
};

class FrameWithModalPopup_p;

/**
 * @brief FrameWithModalPopup enables modal dialog within widget.
 */
class UISE_DESKTOP_EXPORT FrameWithModalPopup : public QFrame
{
    Q_OBJECT

    public:

        static const int DefaultMaxWidthPercent=50;
        static const int DefaultMaxHeightPercent=50;
        static const int DefaultPopupAlpha=150;

        /**
             * @brief Constructor.
             * @param parent Parent widget.
             */
        FrameWithModalPopup(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~FrameWithModalPopup();

        FrameWithModalPopup(const FrameWithModalPopup&)=delete;
        FrameWithModalPopup(FrameWithModalPopup&&)=delete;
        FrameWithModalPopup& operator=(const FrameWithModalPopup&)=delete;
        FrameWithModalPopup& operator=(FrameWithModalPopup&&)=delete;

        /**
         * @brief Popup modal widget.
         * @param widget Widget to show in modal dialog.
         */
        void popup(QWidget* widget);

        /**
         * @brief Close popup widget.
         */
        void closePopup();

        /**
         * @brief Check if modal popup widget is shown.
         * @return Operation result.
         */
        bool isPopupLocked() const;

        /**
         * @brief Set maximal percent of parent frame width that modal popup will take.
         * @param val
         */
        void setMaxWidthPercent(int val);

        /**
         * @brief Get maximal percent of parent frame width that modal popup will take.
         * @return Operation result.
         */
        int maxWidthPercent() const;

        /**
         * @brief Set maximal percent of parent frame height that modal popup will take.
         * @param val
         */
        void setMaxHeightPercent(int val);

        /**
         * @brief Get maximal percent of parent frame height that modal popup will take.
         * @return Operation result.
         */
        int maxHeightPercent() const;

        /**
         * @brief Set alpha channel of modal background color.
         * @param val
         */
        void setPopupAlpha(int val);

        /**
         * @brief Get alpha channel of modal background color.
         * @return Operation result.
         */
        int popupAlpha() const;

    protected:

        void resizeEvent(QResizeEvent *event) override;

    private:

        void setPopupHidden();

        friend class ModalPopup;

        std::unique_ptr<FrameWithModalPopup_p> pimpl;
};


UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_MODAL_POPUP_HPP
