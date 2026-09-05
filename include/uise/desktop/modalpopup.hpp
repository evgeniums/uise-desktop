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

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

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

        void setWidget(QWidget* widget, bool autoDestroy=false);
        void popup();

        void close(bool autoDestroy=true);

        void setShortcutEnabled(bool enable);
        bool isShortcutEnabled() const;

        //! Close when a mouse press lands directly on this frame rather than on its popup
        //! widget child -- since the widget is sized/positioned to cover only its own rect
        //! (see updateWidgetGeometry()), any press this frame itself receives is by
        //! construction outside that widget.
        void setOutsideClickEnabled(bool enable);
        bool isOutsideClickEnabled() const;

    protected:

        void resizeEvent(QResizeEvent *event) override;
        void mousePressEvent(QMouseEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:

        //! close(), unless the popup widget is an AbstractDialog that was marked non-closable:
        //! Escape and the outside click are dismissals performed on the user's behalf, and such
        //! a dialog accepts none of them.
        void closeByUser();

        void updateWidgetGeometry();

        /**
         * @brief ensurePolished() over the popup widget's whole subtree, so a widget that has
         *  never been shown yet is measured with real (post-QSS) geometry rather than
         *  construction defaults. Deliberately NOT Style::repolishRecursive(): unpolish+polish
         *  would re-fire qproperty-* setters that some widgets (e.g. FileUploadWidget) use to
         *  schedule further work, which would undo the point of priming before show.
         */
        void polishWidgetTree();

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
        static const int DefaultPopupAlpha=90;

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
        int getPopupAlpha() const;

        /**
         * @brief Set popup widget.
         * @param widget Widget to show in modal dialog.
         */
        void setPopupWidget(QWidget* widget, bool autoDestroy=false);

        void setShortcutEnabled(bool enable);
        bool isShortcutEnabled() const;

        void setOutsideClickEnabled(bool enable);
        bool isOutsideClickEnabled() const;

        void setAutoColor(bool enable);
        bool isAutoColor() const;

        /**
         * @brief Enable/disable fitting popup height to its content.
         *
         * When enabled the popup height is computed from the content widget's preferred
         * height (heightForWidth of the resolved width, falling back to sizeHint), bounded
         * below by the widget's minimumHeight and above by maxHeightPercent of the parent
         * frame (and by the widget's maximumHeight if a smaller explicit cap is set). The
         * popup also reflows automatically whenever the content requests a new layout, so a
         * dialog that grows (e.g. a wrapped multiline error) resizes to fit while visible.
         * @param enable
         */
        void setPopupAutoHeight(bool enable);

        /**
         * @brief Check whether fitting popup height to content is enabled.
         * @return Operation result.
         */
        bool isPopupAutoHeight() const;

        void setContentWidget(QWidget* widget);

    signals:

        void popupHidden();

    public slots:

        /**
         * @brief Show popup widget.
         */
        void popup();

        /**
         * @brief Close popup widget.
         */
        void closePopup();

    protected:

        void resizeEvent(QResizeEvent *event) override;

    private:

        void setPopupHidden();

        friend class ModalPopup;

        std::unique_ptr<FrameWithModalPopup_p> pimpl;
};


}

#endif // UISE_DESKTOP_MODAL_POPUP_HPP
