#ifndef __info_h__
#define __info_h__


#include <kio/slavebase.h>
#include <QtCore/QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(LOG_KIO_INFO)

class InfoProtocol : public KIO::SlaveBase
{
public:

    InfoProtocol( const QByteArray &pool, const QByteArray &app );
    virtual ~InfoProtocol();

    virtual void get( const QUrl& url );
    virtual void stat( const QUrl& url );
    virtual void mimetype( const QUrl& url );

protected:

    void decodeURL( const QUrl &url );
    void decodePath( QString path );

private:

    QString   m_page;
    QString   m_node;

    QString   m_perl;
    QString   m_infoScript;
    QString   m_infoConf;
    QString   m_cssLocation;
};

#endif // __info_h__
