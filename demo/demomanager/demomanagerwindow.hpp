/**
@copyright Evgeny Sidorov 2026

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)

2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file demo/demomanager/demomanagerwindow.hpp
*
*  Declares DemoManagerWindow.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_DEMO_MANAGER_WINDOW_HPP
#define UISE_DESKTOP_DEMO_MANAGER_WINDOW_HPP

#include <vector>

#include <QMainWindow>
#include <QString>

#include <uise/desktop/uisedesktop.hpp>

class QLineEdit;
class QLabel;
class QVBoxLayout;
class QFrame;

using namespace UISE_DESKTOP_NAMESPACE;

//--------------------------------------------------------------------------

/**
 * @brief Main window of the UISE demo manager.
 *
 * Lists every demo application compiled into the package (see demoRegistry())
 * with its title and a short description, lets the user filter the list, and
 * launches a demo as a separate detached process next to the manager itself
 * -- this works unmodified both in a developer build (all demos share
 * demo/bin/) and in a deployed package (see cmake/uisedemo.cmake and
 * build/unix-deploy.sh).
 */
class DemoManagerWindow : public QMainWindow
{
    Q_OBJECT

    public:

        DemoManagerWindow(QWidget* parent=nullptr);

    private:

        struct DemoRow
        {
            QString executable;
            QString title;
            QString description;
            QFrame* frame;
        };

        void rebuildRows();
        void applyFilter(const QString& text);
        void launchDemo(const QString& executable);
        void toggleTheme(bool dark);

        QLineEdit* m_filterEdit;
        QLabel* m_countLabel;
        QWidget* m_listContent;
        QVBoxLayout* m_listLayout;

        std::vector<DemoRow> m_rows;
};

//--------------------------------------------------------------------------

#endif // UISE_DESKTOP_DEMO_MANAGER_WINDOW_HPP
