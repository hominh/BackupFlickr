#include <QCoreApplication>
#include <qdebug.h>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QtNetwork/QNetworkReply>
#include <cstdio>
#include <QFileInfo>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class QSslError;
using namespace std;

class DownloadManager : public QObject
{
    Q_OBJECT
    QNetworkAccessManager manager;
    QVector<QNetworkReply *> currentDownloads;
public:
    DownloadManager();
    void doDownload(const QUrl &url);
    static QString saveFileName(const QUrl &url);
    bool saveToDisk(const QString &filename, QIODevice *data);
    static bool isHttpRedirect(QNetworkReply *reply);

public slots:
    void execute();
    void downloadFinished(QNetworkReply*);
    void sslErrors(const QList<QSslError> &errors);
};

DownloadManager::DownloadManager()
{
    connect(&manager, SIGNAL(finished(QNetworkReply*)),
                SLOT(downloadFinished(QNetworkReply*)));
}

void DownloadManager::doDownload(const QUrl &url)
{
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

#if QT_CONFIG(ssl)
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
            SLOT(sslErrors(QList<QSslError>)));
#endif
    currentDownloads.append(reply);
}

QString DownloadManager::saveFileName(const QUrl &url)
{
    QString path = url.path();
    QString basename = QFileInfo(path).fileName();

    if(basename.isEmpty())
    {
        basename = "download";
    }
    if(QFileInfo::exists(basename))
    {
        int i = 0;
        basename += '.';
        while (QFile::exists(basename + QString::number(i)))
            ++i;
        basename += QString::number(i);
    }
    return basename;
}

bool DownloadManager::saveToDisk(const QString &filename, QIODevice *data)
{
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly))
    {
        fprintf(stderr, "Could not open %s for writing: %s\n",
                        qPrintable(filename),
                        qPrintable(file.errorString()));
        return false;
    }
    file.write(data->readAll());
    file.close();
    return true;
}

bool DownloadManager::isHttpRedirect(QNetworkReply *reply)
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return statusCode == 301 || statusCode == 302 || statusCode == 303
               || statusCode == 305 || statusCode == 307 || statusCode == 308;
}

void DownloadManager::execute()
{
    QStringList args = QCoreApplication::instance()->arguments();
    args.takeFirst();
    if (args.isEmpty()) {
        printf("Url is empty \n");
        QCoreApplication::instance()->quit();
        return;
    }

    for (const QString &arg : qAsConst(args)) {
        QUrl url = QUrl::fromEncoded(arg.toLocal8Bit());
        doDownload(url);
    }
}

void DownloadManager::sslErrors(const QList<QSslError> &sslErrors)
{
#if QT_CONFIG(ssl)
    for (const QSslError &error : sslErrors)
        fprintf(stderr, "SSL error: %s\n", qPrintable(error.errorString()));
#else
    Q_UNUSED(sslErrors);
#endif
}

void DownloadManager::downloadFinished(QNetworkReply *reply)
{
    QUrl url = reply->url();
    if (reply->error())
    {
        fprintf(stderr, "Download of %s failed: %s\n",
                url.toEncoded().constData(),
                qPrintable(reply->errorString()));
    }
    else
    {
        if (isHttpRedirect(reply))
        {
            fputs("Request was redirected.\n", stderr);
        }
        else
        {
            QString filename = saveFileName(url);
            if (saveToDisk(filename, reply))
            {
                printf("Download of %s succeeded (saved to %s)\n",
                       url.toEncoded().constData(), qPrintable(filename));
            }
        }
    }

    currentDownloads.removeAll(reply);
    reply->deleteLater();

    if (currentDownloads.isEmpty()) {
        // all downloads finished
        QCoreApplication::instance()->quit();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "Start program";

    QEventLoop eventLoop;
    QNetworkAccessManager mgr;
    QObject::connect(&mgr, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    QNetworkRequest req(QUrl("https://api.flickr.com/services/rest/?method=flickr.photosets.getPhotos&api_key=36437bc5fdc0a6f4a03171b5867575eb&photoset_id=72157689328182083&user_id=119669237@N03&extras=url_o&format=json&nojsoncallback=1"));
    QNetworkReply *reply = mgr.get(req);
    eventLoop.exec(); // blocks stack until "finished()" has been called
    QStringList listUrl;
    if (reply->error() == QNetworkReply::NoError) {

        QString strReply = (QString)reply->readAll();


        QJsonDocument jsonResponse = QJsonDocument::fromJson(strReply.toUtf8());

        QJsonObject jsonObject = jsonResponse.object();
        QJsonObject jsonPhotoset = jsonObject["photoset"].toObject();
        QJsonArray jsonPhoto = jsonPhotoset["photo"].toArray();
        for(int i = 0; i < jsonPhoto.count(); i++)
        {
            listUrl.append(jsonPhoto[i].toObject()["url_o"].toString());
        }
        delete reply;
    }
    else {
        //failure
        qDebug() << "Failure" <<reply->errorString();
        delete reply;
    }

    DownloadManager manager;

    QString listTmp;
    for(int i = 0; i < listUrl.count(); i++ )
    {
        listTmp += listUrl[i];
        listTmp += " ";
    }


    QTimer::singleShot(0, &manager, SLOT(execute()));
    return app.exec();

}
#include "main.moc"
