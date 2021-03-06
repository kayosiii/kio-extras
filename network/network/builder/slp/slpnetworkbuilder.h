/*
    This file is part of the Mollet network library, part of the KDE project.

    Copyright 2009 Friedrich W. H. Kossebau <kossebau@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SLPNETWORKBUILDER_H
#define SLPNETWORKBUILDER_H

// lib
#include "abstractnetworkbuilder.h"

namespace Mollet {
class NetworkPrivate;
class SlpServiceBrowser;
class SLPService;
}
template <class T> class QList;


namespace Mollet
{

class SlpNetworkBuilder : public AbstractNetworkBuilder
{
    Q_OBJECT

  public:
    explicit SlpNetworkBuilder( NetworkPrivate* networkPrivate );
    virtual ~SlpNetworkBuilder();

  public:

  private Q_SLOTS:
    void onServicesAdded( const QList<SLPService>& services );
    void onServicesChanged( const QList<SLPService>& services );
    void onServicesRemoved( const QList<SLPService>& services );

  private:
    NetworkPrivate* mNetworkPrivate;

    SlpServiceBrowser* mSlpServiceBrowser;
};

}

#endif
