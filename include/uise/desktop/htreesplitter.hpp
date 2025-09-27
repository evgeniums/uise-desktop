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

/** @file uise/desktop/htreesplitter.hpp
*
*  Declares splitter for horizontal tree.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_HTREE_SPLITTER_HPP
#define UISE_DESKTOP_HTREE_SPLITTER_HPP

#include <memory>

#include <QFrame>

#include <uise/desktop/uisedesktop.hpp>

#include <uise/desktop/htreepath.hpp>

class QScrollBar;

UISE_DESKTOP_NAMESPACE_BEGIN

class HTreeSplitter_p;

class UISE_DESKTOP_EXPORT HTreeSplitter : public QFrame
{
    Q_OBJECT

    public:

        constexpr static const int DefaultMinWidth=100;
        constexpr static const int SectionLineWidth=2;

        /**
         * @brief Constructor.
         * @param parent Parent widget.
         */
        HTreeSplitter(QWidget* parent=nullptr);

        /**
         * @brief Destructor.
         */
        ~HTreeSplitter();

        HTreeSplitter(const HTreeSplitter&)=delete;
        HTreeSplitter(HTreeSplitter&&)=delete;
        HTreeSplitter& operator=(const HTreeSplitter&)=delete;
        HTreeSplitter& operator=(HTreeSplitter&&)=delete;

        void addWidget(QWidget* widget, int stretch=0, bool scrollTo=true);
        QWidget* widget(int index) const;
        void removeWidget(int index);

        int count() const;

        QWidget* viewPort() const;

        int sectionLineWidth() const
        {
            return SectionLineWidth;
        }

        void truncate(int index);

    public slots:

        void scrollToEnd();
        void scrollToHome();
        void scrollToWidget(QWidget* widget, int xmargin = 50);
        void scrollToIndex(int index, int xmargin = 50);

    public slots:

        void toggleSectionExpanded(int index, bool expanded, bool visible);

    private:

        std::unique_ptr<HTreeSplitter_p> pimpl;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_HTREE_SPLITTER_HPP
