/* This file is part of the KDE project
   Copyright (c) 2004 Kevin Ottens <ervin ipsquad net>

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef REMOTEIMPL_H
#define REMOTEIMPL_H

#include <kio/global.h>
#include <kio/job.h>
#include <kurl.h>

#include <qstring.h>

class RemoteImpl
{
public:
	RemoteImpl();

	void createTopLevelEntry(KIO::UDSEntry &entry) const;
	bool createWizardEntry(KIO::UDSEntry &entry) const;
	bool isWizardURL(const KURL &url) const;
	bool statNetworkFolder(KIO::UDSEntry &entry, const QString &filename) const;

	void listRoot(QValueList<KIO::UDSEntry> &list) const;

	KURL findBaseURL(const QString &filename) const;
	
	bool deleteNetworkFolder(const QString &filename) const;

private:
	bool findDirectory(const QString &filename, QString &directory) const;
	void createEntry(KIO::UDSEntry& entry, const QString &directory,
	                 const QString &file) const;
};

#endif