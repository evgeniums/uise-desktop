/**
@copyright Evgeny Sidorov 2022-2025

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/circlebusyframe.hpp
*
*  Declares CircleBusyFrame.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_CIRCLE_BUSY_FRAME_HPP
#define UISE_DESKTOP_CIRCLE_BUSY_FRAME_HPP

#include <QPointer>

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractloadingwidget.hpp>

// Written as the literal namespace, not the UISE_DESKTOP_NAMESPACE_BEGIN macro: lupdate cannot expand a macro-opened
// namespace, so it records tr() calls in this file under an unqualified context that does not
// match what moc (a real preprocessor) resolves at runtime -- translations for every string here
// would silently stay in English. Do not revert to the macro form. See task-localization-framework.md.
namespace uise {

class CircleBusy;

/**
 * @brief Frame wrapping CircleBusy that implements AbstractOperationLoadingWidget.
 */
class UISE_DESKTOP_EXPORT CircleBusyFrame : public AbstractOperationLoadingWidget
{
    Q_OBJECT

    public:

        explicit CircleBusyFrame(QWidget* parent=nullptr,
                                 bool centerOnParent=true,
                                 bool disableParentWhenSpinning=false);

        ~CircleBusyFrame() override;

        CircleBusy* circleBusyWidget() const;

        bool isRunning() const noexcept override;

    public slots:

        void start() override;
        void stop() override;

    private:

        QPointer<CircleBusy> m_circleBusy;
};

}

#endif // UISE_DESKTOP_CIRCLE_BUSY_FRAME_HPP
