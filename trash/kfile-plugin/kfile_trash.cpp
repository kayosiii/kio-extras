/* This file is part of the KDE project
 * Copyright (C) 2004 David Faure <faure@kde.org>
 *     Based on kfile_txt.cpp by Nadeem Hasan <nhasan@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "kfile_trash.h"

#include <kgenericfactory.h>
#include <kdebug.h>

#include <qfile.h>
#include <qstringlist.h>
#include <qdatetime.h>

typedef KGenericFactory<KTrashPlugin> TrashFactory;

K_EXPORT_COMPONENT_FACTORY(kfile_trash, TrashFactory("kfile_trash"))

KTrashPlugin::KTrashPlugin(QObject *parent, const char *name,
        const QStringList &args) : KFilePlugin(parent, name, args)
{
    KGlobal::locale()->insertCatalogue( "kio_trash" );

    kdDebug(7034) << "Trash file meta info plugin\n";
    KFileMimeTypeInfo* info = addMimeTypeInfo( "trash" );

    KFileMimeTypeInfo::GroupInfo* group =
            addGroupInfo(info, "General", i18n("General"));

    KFileMimeTypeInfo::ItemInfo* item;
    item = addItemInfo(group, "OriginalPath", i18n("Original Path"), QVariant::String);
    item = addItemInfo(group, "DateOfDeletion", i18n("Date of Deletion"), QVariant::DateTime);

    (void)impl.init();
}

bool KTrashPlugin::readInfo(KFileMetaInfo& info, uint)
{
    //kdDebug() << k_funcinfo << info.url() << endl;
    if ( info.url().protocol() != "trash" )
        return false;

    int trashId;
    QString fileId;
    QString relativePath;
    if ( !TrashImpl::parseURL( info.url(), trashId, fileId, relativePath ) )
        return false;

    TrashImpl::TrashedFileInfo trashInfo;
    if ( !impl.infoForFile( trashId, fileId, trashInfo ) )
        return false;

    KFileMetaInfoGroup group = appendGroup(info, "General");
    appendItem(group, "OriginalPath", trashInfo.origPath);
    appendItem(group, "DateOfDeletion", trashInfo.deletionDate);

    return true;
}

#include "kfile_trash.moc"