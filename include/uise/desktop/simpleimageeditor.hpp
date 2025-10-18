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

/** @file uise/desktop/simpleimageeditor.hpp
*
*  Declares SimpleImageEditor.
*
*/

/****************************************************************************/

#ifndef UISE_DESKTOP_SIMPLE_IMAGE_EDITOR_HPP
#define UISE_DESKTOP_SIMPLE_IMAGE_EDITOR_HPP

#include <uise/desktop/uisedesktop.hpp>
#include <uise/desktop/abstractimageeditor.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

class SimpleImageEditorWidget;

class UISE_DESKTOP_EXPORT SimpleImageEditor : public AbstractImageEditor
{
    Q_OBJECT

    public:

        using AbstractImageEditor::AbstractImageEditor;

        QPixmap editedImage() override;

    public slots:

        void zoomIn() override;
        void zoomOut() override;
        void flipVertical() override;
        void flipHorizontal() override;
        void rotate() override;
        void rotateClockwise() override;

    protected:

        void updateCropShape() override;

        void updateImageSizeLimits() override;

        void updateAspectRatio() override;

        void doLoadImage() override;

        void updateFilenameState() override;

        Widget* doCreateActualWidget(QWidget* parent) override;

    private:

        void doUpdateFilenameState();
        void resetCropper();

        SimpleImageEditorWidget* m_widget;
};

class SimpleImageEditorWidget_p;
class UISE_DESKTOP_EXPORT SimpleImageEditorWidget : public WidgetQFrame
{
    Q_OBJECT

    public:

        SimpleImageEditorWidget(SimpleImageEditor* ctrl, QWidget* parent=nullptr);

        ~SimpleImageEditorWidget();
        SimpleImageEditorWidget(const SimpleImageEditorWidget&)=delete;
        SimpleImageEditorWidget(SimpleImageEditorWidget&&)=delete;
        SimpleImageEditorWidget& operator=(const SimpleImageEditorWidget&)=delete;
        SimpleImageEditorWidget& operator=(SimpleImageEditorWidget&&)=delete;

    private:

        std::unique_ptr<SimpleImageEditorWidget_p> pimpl;

        friend class SimpleImageEditor;
};

UISE_DESKTOP_NAMESPACE_END

#endif // UISE_DESKTOP_SIMPLE_IMAGE_EDITOR_HPP
