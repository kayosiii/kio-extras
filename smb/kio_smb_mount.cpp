/*  This file is part of the KDE project

    Copyright (C) 2000 Alexander Neundorf <neundorf@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kio_smb.h"

#include <unistd.h>
#include <QByteArray>
#include <QDir>
#include <QDataStream>
#include <KLocalizedString>
#include <KProcess>
#include <kshell.h>

void SMBSlave::special( const QByteArray & data)
{
   qCDebug(KIO_SMB)<<"Smb::special()";
   int tmp;
   QDataStream stream(data);
   stream >> tmp;
   //mounting and umounting are both blocking, "guarded" by a SIGALARM in the future
   switch (tmp)
   {
   case 1:
   case 3:
      {
         QString remotePath, mountPoint, user;
         stream >> remotePath >> mountPoint;

         QStringList sl=remotePath.split('/');
         QString share,host;
         if (sl.count()>=2)
         {
            host=sl.at(0).mid(2);
            share=sl.at(1);
            qCDebug(KIO_SMB)<<"special() host -"<< host <<"- share -" << share <<"-";
         }

         remotePath.replace('\\', '/');  // smbmounterplugin sends \\host/share

         qCDebug(KIO_SMB) << "mounting: " << remotePath.toLocal8Bit() << " to " << mountPoint.toLocal8Bit();

         if (tmp==3) {
             if (!QDir().mkpath(mountPoint)) {
                 error(KIO::ERR_COULD_NOT_MKDIR, mountPoint);
                 return;
             }
         }

         SMBUrl smburl(QUrl("smb:///"));
         smburl.setHost(host);
         smburl.setPath('/' + share);

         if ( !checkPassword(smburl) )
         {
           finished();
           return;
         }

         // using smbmount instead of "mount -t smbfs", because mount does not allow a non-root
         // user to do a mount, but a suid smbmnt does allow this

         KProcess proc;
         proc.setOutputChannelMode(KProcess::SeparateChannels);
         proc << "smbmount";

         QString options;

         if ( smburl.userName().isEmpty() )
         {
           user = "guest";
           options = "guest";
         }
         else
         {
           options = "username=" + smburl.userName();
           user = smburl.userName();

           if ( ! smburl.password().isEmpty() )
             options += ",password=" + smburl.password();
         }

         // TODO: check why the control center uses encodings with a blank char, e.g. "cp 1250"
         //if ( ! m_default_encoding.isEmpty() )
           //options += ",codepage=" + KShell::quoteArg(m_default_encoding);

         proc << remotePath;
         proc << mountPoint;
         proc << "-o" << options;

         proc.start();
         if (!proc.waitForFinished())
         {
            error(KIO::ERR_CANNOT_LAUNCH_PROCESS,
                  "smbmount"+i18n("\nMake sure that the samba package is installed properly on your system."));
            return;
         }

         QString mybuf = QString::fromLocal8Bit(proc.readAllStandardOutput());
         QString mystderr = QString::fromLocal8Bit(proc.readAllStandardError());

         qCDebug(KIO_SMB) << "mount exit " << proc.exitCode()
                          << "stdout:" << mybuf << endl << "stderr:" << mystderr << endl;

         if (proc.exitCode() != 0)
         {
           error( KIO::ERR_COULD_NOT_MOUNT,
               i18n("Mounting of share \"%1\" from host \"%2\" by user \"%3\" failed.\n%4",
                share, host, user, mybuf + '\n' + mystderr));
           return;
         }

         finished();
      }
      break;
   case 2:
   case 4:
      {
         QString mountPoint;
         stream >> mountPoint;

         KProcess proc;
         proc.setOutputChannelMode(KProcess::SeparateChannels);
         proc << "smbumount";
         proc << mountPoint;

         proc.start();
         if ( !proc.waitForFinished() )
         {
           error(KIO::ERR_CANNOT_LAUNCH_PROCESS,
                 "smbumount"+i18n("\nMake sure that the samba package is installed properly on your system."));
           return;
         }

         QString mybuf = QString::fromLocal8Bit(proc.readAllStandardOutput());
         QString mystderr = QString::fromLocal8Bit(proc.readAllStandardError());

         qCDebug(KIO_SMB) << "smbumount exit " << proc.exitCode()
                          << "stdout:" << mybuf << endl << "stderr:" << mystderr << endl;

         if (proc.exitCode() != 0)
         {
           error(KIO::ERR_COULD_NOT_UNMOUNT,
               i18n("Unmounting of mountpoint \"%1\" failed.\n%2",
                mountPoint, mybuf + '\n' + mystderr));
           return;
         }

         if ( tmp == 4 )
         {
           bool ok;

           QDir dir(mountPoint);
           dir.cdUp();
           ok = dir.rmdir(mountPoint);
           if ( ok )
           {
             QString p=dir.path();
             dir.cdUp();
             ok = dir.rmdir(p);
           }

           if ( !ok )
           {
             error(KIO::ERR_COULD_NOT_RMDIR, mountPoint);
             return;
           }
         }

         finished();
      }
      break;
   default:
      break;
   }
   finished();
}
