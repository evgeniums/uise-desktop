/**
@copyright Evgeny Sidorov 2022

This software is dual-licensed. Choose the appropriate license for your project.

1. The GNU GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-GPLv3.md](LICENSE-GPLv3.md) or copy at https://www.gnu.org/licenses/gpl-3.0.txt)
    
2. The GNU LESSER GENERAL PUBLIC LICENSE, Version 3.0
     (see accompanying file [LICENSE-LGPLv3.md](LICENSE-LGPLv3.md) or copy at https://www.gnu.org/licenses/lgpl-3.0.txt).

You may select, at your option, one of the above-listed licenses.

*/

/****************************************************************************/

/** @file uise/desktop/svgiconcontext.hpp
*
*  Defines Icon.
*
*/

/****************************************************************************/

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <uise/desktop/svgiconcontext.hpp>

UISE_DESKTOP_NAMESPACE_BEGIN

namespace {

QString nameWithContext(const QString& name, const QString& context)
{
    return context + "::" + name;
}

}

//--------------------------------------------------------------------------

bool SvgIconTheme::loadFromJson(const QString& json, QString* errorMessage)
{
    QJsonParseError ec;
    auto src=json.toUtf8();

    auto doc=QJsonDocument::fromJson(src, &ec);
    if (doc.isNull())
    {
        if (errorMessage!=nullptr)
        {
            *errorMessage=ec.errorString();
        }
        return false;
    }

    if (!doc.isObject())
    {
        if (errorMessage!=nullptr)
        {
            *errorMessage=tr("json theme must be a JSON object");
        }
        return false;
    }

    auto formatError=[errorMessage](const QString& msg, const QStringList& path)
    {
        if (errorMessage!=nullptr)
        {
            *errorMessage=QString(tr("%1 at path %2").arg(msg,path.join(".")));
        }
        return false;
    };

    auto mustBeArray=[&formatError](const QStringList& path)
    {
        return formatError(tr("must be JSON array"),path);
    };

    auto mustBeObject=[&formatError](const QStringList& path)
    {
        return formatError(tr("must be JSON object"),path);
    };

    auto mustBeString=[&formatError](const QStringList& path)
    {
        return formatError(tr("must be string"),path);
    };

    auto obj=doc.object();

    // extract theme name
    auto nameEl=obj.value("name");
    if (!nameEl.isString())
    {
        return mustBeString(QStringList("name"));
    }
    m_name=nameEl.toString();

    // extract theme contexts
    auto contexts=obj.value("contexts");
    if (!contexts.isArray())
    {
        return mustBeArray({"contexts"});
    }

    // iterate contexts
    auto contextsArr=contexts.toArray();
    for (qsizetype i=0;i<contextsArr.count();i++)
    {
        QStringList path{QString::number(i)};

        auto context=contextsArr.at(i);
        if (context.isNull())
        {
            continue;
        }

        if (!context.isObject())
        {
            return mustBeObject(path);
        }

        auto ctxObj=context.toObject();

        // context name
        auto nameEl=ctxObj.value("name");
        if (!nameEl.isString())
        {
            return mustBeString(path+QStringList("name"));
        }
        SvgIconContext ctx;
        ctx.name=nameEl.toString();

        // tags
        if (ctxObj.contains("tags"))
        {
            auto tags=ctxObj.value("tags");
            if (!tags.isArray())
            {
                return mustBeArray(path);
            }
            auto tagsArr=tags.toArray();
            for (qsizetype j=0;i<tagsArr.count();j++)
            {
                auto tag=tagsArr.at(j);
                if (!tag.isString())
                {
                    return mustBeString(path + QStringList{QString::number(j)});
                }
                auto tagStr=tag.toString();
                ctx.tags.insert(tagStr);
            }
        }

        auto parseStringMap=[i,&mustBeObject,&mustBeString](std::map<QString,QString>& map, const QString& field, QJsonObject& obj, QStringList path, const QString& keyContext={})
        {
            if (obj.contains(field))
            {
                path.append(field);

                auto fieldVal=obj.value(field);
                if (!fieldVal.isObject())
                {
                    return mustBeObject(path);
                }

                auto fieldValObj=fieldVal.toObject();
                for (auto it1=fieldValObj.begin();it1!=fieldValObj.end();++it1)
                {
                    auto key=it1.key();
                    auto val=it1.value();
                    if (!val.isString())
                    {
                        path.append(key);
                        return mustBeString(path);
                    }
                    auto str=val.toString();
                    if (!str.isEmpty())
                    {
                        if (keyContext.isEmpty())
                        {
                            map.emplace(key,str);
                        }
                        else
                        {
                            map.emplace(nameWithContext(key,keyContext),str);
                        }
                    }
                }
            }

            return true;
        };

        // aliases
        QString aliasesField{"aliases"};
        auto ok=parseStringMap(ctx.aliases,aliasesField,ctxObj,path,ctx.name);
        if (!ok)
        {
            return false;
        }

        // name paths
        QString pathsField{"paths"};
        ok=parseStringMap(ctx.namePaths,pathsField,ctxObj,path,ctx.name);
        if (!ok)
        {
            return false;
        }

        auto parseColorMaps=[this,&parseStringMap,&mustBeObject,&formatError](const QString& field, QStringList path, const QJsonObject& obj, SvgIconColorMaps modes)
        {
            if (obj.contains(field))
            {
                path.append(field);

                auto fieldVal=obj.value(field);
                if (!fieldVal.isObject())
                {
                    return mustBeObject(path);
                }

                auto fieldValObj=fieldVal.toObject();
                for (auto it1=fieldValObj.begin();it1!=fieldValObj.end();++it1)
                {
                    auto key=it1.key();
                    auto mpath=path+QStringList{key};

                    auto mod=mode(key);
                    if (!mod)
                    {
                        return formatError(tr("invalid icon mode"),mpath);
                    }

                    SvgIcon::ColorMap colorMaps;

                    auto val=it1.value();
                    if (!val.isObject())
                    {
                        return formatError(tr("invalid value for icon mode"),mpath);
                    }
                    auto colorMapObj=val.toObject();

                    // on colors
                    bool ok=parseStringMap(colorMaps.on,"on",colorMapObj,mpath);
                    if (!ok)
                    {
                        return false;
                    }

                    // off colors
                    ok=parseStringMap(colorMaps.off,"off",colorMapObj,mpath);
                    if (!ok)
                    {
                        return false;
                    }

                    // emplace to map
                    modes.emplace(mod.value_or(IconMode::Normal),std::move(colorMaps));
                }
            }

            // done
            return true;
        };

        // color modes
        QString modesField="modes";
        ok=parseColorMaps(modesField,path,ctxObj,ctx.modes);
        if (!ok)
        {
            return false;
        }

        // explicit icon configs
        QString iconsField="icons";
        if (ctxObj.contains(iconsField))
        {
            auto iconsPath=path + QStringList{iconsField};

            auto fieldVal=ctxObj.value(iconsField);
            if (!fieldVal.isArray())
            {
                return mustBeArray(iconsPath);
            }

            auto iconsArr=fieldVal.toArray();
            for (qsizetype j=0;j<iconsArr.count();j++)
            {
                auto iconPath=iconsPath + QStringList{QString::number(j)};

                auto iconVal=iconsArr.at(j);
                if (!iconVal.isObject())
                {
                    return mustBeObject(iconPath);
                }
                auto iconObj=iconVal.toObject();

                // icon name
                QString iconNameField{"name"};
                auto iconNameVal=iconObj.value(iconNameField);
                if (!iconNameVal.isString())
                {
                    return mustBeString(iconPath+QStringList{iconNameField});
                }
                auto iconName=iconNameVal.toString();
                if (!iconNameVal.isString())
                {
                    return formatError(tr("must be not empty"),iconPath+QStringList{iconNameField});
                }

                SvgIconConfig icon{nameWithContext(iconName,ctx.name)};

                // icon color maps
                ok=parseColorMaps(modesField,iconPath,iconObj,icon.modes);
                if (!ok)
                {
                    return false;
                }

                // icon modes
                ok=parseColorMaps(modesField,iconPath,iconObj,icon.modes);
                if (!ok)
                {
                    return false;
                }

                // icon aliases
                if (iconObj.contains(aliasesField))
                {
                    auto aliasesVal=iconObj.value(aliasesField);
                    auto aliasesPath=iconPath+QStringList{aliasesField};
                    if (!aliasesVal.isObject())
                    {
                        return mustBeObject(aliasesPath);
                    }
                    auto aliasesObj=aliasesVal.toObject();
                    for (auto it1=aliasesObj.begin();it1!=aliasesObj.end();++it1)
                    {
                        auto key=it1.key();
                        if (!key.isEmpty())
                        {
                            auto aliasPath=path + QStringList{key};
                            auto aliasVal=it1.value();
                            if (!aliasVal.isArray())
                            {
                                return mustBeArray(aliasPath);
                            }
                            std::set<IconVariant> modes;
                            auto aliasArr=aliasVal.toArray();
                            for (qsizetype k=0;k<aliasArr.count();k++)
                            {
                                auto aliasModePath=aliasPath + QStringList{QString::number(k)};
                                auto aliasModeVal=aliasArr.at(k);
                                if (!aliasModeVal.isString())
                                {
                                    return mustBeString(aliasModePath);
                                }
                                auto aliasModeStr=aliasModeVal.toString();
                                auto aliasMode=mode(aliasModeStr);
                                if (!aliasMode)
                                {
                                    return formatError(tr("invalid icon mode"),aliasModePath);
                                }
                                modes.insert(aliasMode.value_or(IconMode::Normal));
                            }
                            icon.aliases.emplace(nameWithContext(key,ctx.name),std::move(modes));
                        }
                    }
                }

                // icon sizes
                QString sizesField{"sizes"};
                if (iconObj.contains(sizesField))
                {
                    auto sizesVal=iconObj.value(sizesField);
                    auto sizesPath=iconPath+QStringList{sizesField};
                    if (!sizesVal.isArray())
                    {
                        return mustBeObject(sizesPath);
                    }
                    auto sizesArr=sizesVal.toArray();
                    for (qsizetype k=0;k<sizesArr.count();k++)
                    {
                        auto sizePath=sizesPath + QStringList{QString::number(k)};
                        auto sizeVal=sizesArr.at(k);
                        if (!sizeVal.isObject())
                        {
                            return mustBeObject(sizePath);
                        }
                        auto sizeObj=sizeVal.toObject();
                        int width=0;
                        int height=0;
                        bool hasWidth=false;
                        if (sizeObj.contains("width"))
                        {
                            auto widthVal=sizeObj.value("width");
                            if (!widthVal.isDouble())
                            {
                                return formatError(tr("invalid number"),sizesPath+QStringList{"width"});
                            }
                            width=widthVal.toInt();
                            height=width;
                            hasWidth=true;
                        }
                        if (sizeObj.contains("height"))
                        {
                            auto heightVal=sizeObj.value("height");
                            if (!heightVal.isDouble())
                            {
                                return formatError(tr("invalid number"),sizesPath+QStringList{"height"});
                            }
                            height=heightVal.toInt();
                            if (!hasWidth)
                            {
                                width=height;
                            }
                        }

                        icon.sizes.insert(QSize{width,height});
                    }
                }
            }
        }
    }

    // done
    return true;
}

//--------------------------------------------------------------------------

UISE_DESKTOP_NAMESPACE_END
