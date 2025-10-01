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

/** @file demo/avatar/avatar.cpp
*
*  Demo application of Avatar.
*
*/

/****************************************************************************/

#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QFile>
#include <QScrollArea>

#include <uise/desktop/utils/layout.hpp>
#include <uise/desktop/avatar.hpp>
#include <uise/desktop/roundedimage.hpp>

using namespace UISE_DESKTOP_NAMESPACE;

struct AvatarBuilder
{
    std::shared_ptr<Avatar> operator() (const QString& name) const
    {
        auto avatar=std::make_shared<Avatar>();

        auto fileName=QString(":/uise/desktop/demo/avatar/assets/%1").arg(name);

        if (QFile::exists(fileName))
        {
            QPixmap px{fileName};
            avatar->setBasePixmap(px);
        }
        if (name.size()<5)
        {
            avatar->setAvatarName(QString("Name %1").arg(name).toStdString());
        }
        else
        {
            avatar->setAvatarName(name.toStdString());
        }

        return avatar;
    }
};

//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc,argv);

    QMainWindow w;
    auto mainFrame=new QScrollArea();
    mainFrame->setWidgetResizable(true);

    auto avatarsFrame=new QFrame(mainFrame);
    auto avatarsLayout=Layout::grid(avatarsFrame,false);
    mainFrame->setWidget(avatarsFrame);

    auto avatarSource=std::make_shared<AvatarSource>();
    avatarSource->setAvatarBuilder(AvatarBuilder{});

    std::vector<QString> names{"1.jpg","2.jpg","unknown.jpg","other unknown.jpg","3.jpg","one more missing.png","александр сергеевич.png"};
    std::vector<QSize> sizes{QSize{64,64},QSize{128,128},QSize{150,150}};

    for (size_t i=0;i<names.size();i++)
    {
        for (size_t j=0;j<sizes.size();j++)
        {
            auto size=sizes.at(j);

            auto img=new RoundedImage(avatarsFrame);
            img->setImageSource(avatarSource,names[i],size);

            avatarsLayout->addWidget(img,i,j);
        }
    }

    w.setCentralWidget(mainFrame);
    w.resize(600,800);
    w.setWindowTitle("Avatar Demo");
    w.show();
    auto ret=app.exec();
    return ret;
}

//--------------------------------------------------------------------------
