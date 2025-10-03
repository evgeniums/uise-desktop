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
    std::shared_ptr<Avatar> operator() (const WithPath& path) const
    {
        auto avatar=std::make_shared<Avatar>();
        auto name=QString::fromUtf8(path.path().front());
        if (name=="noname")
        {
            name.clear();
        }

        if (!name.isEmpty())
        {
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
    auto noNameSvgIcon=std::make_shared<SvgIcon>();
    noNameSvgIcon->addFile(":/icons/tabler-icons/filled/user.svg");
    avatarSource->setNoNameSvgIcon(std::move(noNameSvgIcon));

    auto avatarSourceRounded=std::make_shared<AvatarSource>();
    avatarSourceRounded->setAvatarBuilder(AvatarBuilder{});
    avatarSourceRounded->setRadiusRatio(0.2);

    std::vector<std::string> names{"1.jpg","2.jpg","unknown.jpg","other unknown.jpg","3.jpg","noname","one more missing.png","александр сергеевич.png"};
    std::vector<QSize> sizes{QSize{64,64},QSize{128,128},QSize{150,150}};

    for (size_t i=0;i<names.size();i++)
    {
        auto name=names[i];

        for (size_t j=0;j<sizes.size();j++)
        {
            auto size=sizes.at(j);

            auto img=new AvatarWidget(avatarsFrame);
            if (i!=3)
            {
                img->setAvatarSource(avatarSource);
            }
            else
            {
                img->setAvatarSource(avatarSourceRounded);
            }
            img->setAvatarPath(name);
            img->setAvatarSize(size);

            if (name!="noname")
            {
                if (name.size()<5)
                {
                    img->setAvatarName(QString("Name %1").arg(QString::fromUtf8(name)).toStdString());
                }
                else
                {
                    img->setAvatarName(name);
                }
            }

            avatarsLayout->addWidget(img,i,j);

            if (i==2)
            {
                img->setRightBottomCircle(true);
                img->setCornerImageSizeRatio(0.2);
                if (size.width()==64)
                {
                    img->setCornerImageOffsets(4,4);
                }
                else if (size.width()==128)
                {
                    img->setRightBottomCircleRadius(10);
                    img->setCornerImageOffsets(8,8);
                }
                else if (size.width()==150)
                {
                    img->setRightBottomCircleRadius(10);
                    img->setCornerImageOffsets(9,9);
                }

                img->setRightBottomCircleColor("#38b000");
            }
            if (i==3)
            {
                auto svgIcon=std::make_shared<SvgIcon>();
                svgIcon->addFile(":/uise/icons/ext/classic/cpp.svg");
                img->setRightBottomSvgIcon(std::move(svgIcon));
            }
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
