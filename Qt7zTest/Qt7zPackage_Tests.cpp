#include <QDir>
#include <QImage>
#include <QScopedPointer>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>

#include "../Qt7z/qt7zfileinfo.h"
#include "../Qt7z/qt7zpackage.h"

class TestQt7zPackage : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

    void openAndClose();
    void openAndClose_data();
#ifdef Q_OS_WIN
    void openAndClosePasswordFilename();
#endif
    void getFileNameList();
    void getFileNameList_data();
    void fileInfoList();
    void fileInfoList_data();
    void extractFile_text();
    void extractFile_text_data();
    void extractFile_image();
    void extractFile_image_data();
    void extractFile_wrongPassword();
    void extractFile_wrongPassword_data();

private:
    QString extractResource(const QString &path);

    QTemporaryDir m_tempDir;
};

class TestQt7zPackageClient : public Qt7zPackage::Client
{
public:
    QString password;

    virtual void openPasswordRequired(QString &requestedPassword) override
    {
        requestedPassword = password;
    }

    virtual void extractPasswordRequired(QString &requestedPassword) override
    {
        requestedPassword = password;
    }
};

void TestQt7zPackage::initTestCase()
{
    Q_INIT_RESOURCE(Qt7zTest);

    if (!m_tempDir.isValid()) {
        QFAIL("Cannot create temporary directory for test materials");
    }
}

QString TestQt7zPackage::extractResource(const QString &path)
{
    QFile r(path);
    if (!r.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QFileInfo info(path);
    const QString &fileName = info.fileName();

    QFile w(m_tempDir.path() + "/" + fileName);
    if (!w.open(QIODevice::WriteOnly)) {
        return QString();
    }

    w.write(r.readAll());
    w.close();

    return w.fileName();
}

void TestQt7zPackage::openAndClose()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(bool, valid);

    const QString &localPkgPath = extractResource(rcPkgPath);

    Qt7zPackage pkg(localPkgPath);
    QCOMPARE(pkg.open(), valid);
    QCOMPARE(pkg.isOpen(), valid);
    pkg.close();
    QVERIFY(!pkg.isOpen());
}

void TestQt7zPackage::openAndClose_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<bool>("valid");

    QTest::newRow("text.7z")
        << ":/assets/text.7z"
        << true;
#ifdef Q_OS_WIN
    QTest::newRow("password.7z")
        << ":/assets/password.7z"
        << true;
#endif
    QTest::newRow("file not exists")
        << "foo"
        << false;
}

#ifdef Q_OS_WIN
void TestQt7zPackage::openAndClosePasswordFilename()
{
    const QString &localPkgPath = extractResource(":/assets/password-filename.7z");

    Qt7zPackage pkg(localPkgPath);
    TestQt7zPackageClient client;
    pkg.setClient(&client);

    QVERIFY(!pkg.open());

    client.password = "foobar";
    QVERIFY(pkg.open());

    pkg.close();
    QVERIFY(!pkg.isOpen());
}
#endif

void TestQt7zPackage::getFileNameList()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QString, password);
    QFETCH(QStringList, expectFileNameList);

    const QString &localPkgPath = extractResource(rcPkgPath);

    Qt7zPackage pkg(localPkgPath);
    TestQt7zPackageClient client;
    pkg.setClient(&client);
    client.password = password;

    QVERIFY(pkg.open());

    const QStringList &actualFileNameList = pkg.getFileNameList();
    QCOMPARE(actualFileNameList.count(), expectFileNameList.count());

    foreach (const QString &actualFileName, actualFileNameList) {
        QVERIFY(expectFileNameList.contains(actualFileName));
    }

    QCOMPARE(pkg.getFileNameList(), pkg.fileNameList());
}

void TestQt7zPackage::getFileNameList_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<QString>("password");
    QTest::addColumn<QStringList>("expectFileNameList");

    QTest::newRow("text.7z")
        << ":/assets/text.7z"
        << ""
        << (QStringList() << "1.txt" << "2.txt" << "sub/1.txt");
#ifdef Q_OS_WIN
    QTest::newRow("password.7z")
        << ":/assets/password.7z"
        << ""
        << (QStringList() << "1.txt" << "yellow.png" << "sub" << "sub/1.txt");
    QTest::newRow("password-filename.7z")
        << ":/assets/password-filename.7z"
        << "foobar"
        << (QStringList() << "1.txt" << "yellow.png" << "sub" << "sub/1.txt");
#endif
}

void TestQt7zPackage::fileInfoList()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QString, password);
    QFETCH(QStringList, expFileNames);
    QFETCH(QList<quint64>, expFileSizes);
    QFETCH(QList<bool>, expIsDirs);
    QFETCH(QList<bool>, expIsCrcDefineds);
    QFETCH(QList<quint32>, expCrcs);

    const QString &localPkgPath = extractResource(rcPkgPath);

    Qt7zPackage pkg(localPkgPath);
    TestQt7zPackageClient client;
    pkg.setClient(&client);
    client.password = password;

    QVERIFY(pkg.open());

    const QList<Qt7zFileInfo> &fileInfos = pkg.fileInfoList();
    QCOMPARE(fileInfos.count(), expFileNames.count());

    for (int i = 0; i < fileInfos.count(); ++i) {
        const Qt7zFileInfo &fileInfo = fileInfos[i];

        QCOMPARE(fileInfo.fileName, expFileNames[i]);
        QCOMPARE(fileInfo.size, expFileSizes[i]);
        QCOMPARE(fileInfo.isDir, expIsDirs[i]);
        QCOMPARE(fileInfo.isCrcDefined, expIsCrcDefineds[i]);
        QCOMPARE(fileInfo.crc, expCrcs[i]);
    }
}

void TestQt7zPackage::fileInfoList_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<QString>("password");
    QTest::addColumn<QStringList>("expFileNames");
    QTest::addColumn<QList<quint64> >("expFileSizes");
    QTest::addColumn<QList<bool> >("expIsDirs");
    QTest::addColumn<QList<bool> >("expIsCrcDefineds");
    QTest::addColumn<QList<quint32> >("expCrcs");

    QTest::newRow("text.7z")
        << ":/assets/text.7z"
        << ""
        << (QStringList() << "1.txt" << "2.txt" << "sub/1.txt")
        << (QList<quint64>() << 10 << 10 << 10)
        << (QList<bool>() << false << false << false)
        << (QList<bool>() << true << true << true)
        << (QList<quint32>() << 2534262748 << 2696674444 << 2534262748);
    QTest::newRow("mixed.7z")
        << ":/assets/mixed.7z"
        << ""
        << (QStringList() << "yellow.png" << "1.txt" << "sub/1.txt" << "sub")
        << (QList<quint64>() << 4323 << 10 << 10 << 0)
        << (QList<bool>() << false << false << false << true)
        << (QList<bool>() << true << true << true << false)
        << (QList<quint32>() << 2786803049 << 2534262748 << 2534262748 << 0);
#ifdef Q_OS_WIN
    QTest::newRow("password.7z")
        << ":/assets/password.7z"
        << ""
        << (QStringList() << "sub" << "1.txt" << "sub/1.txt" << "yellow.png")
        << (QList<quint64>() << 0 << 10 << 10 << 4323)
        << (QList<bool>() << true << false << false << false)
        << (QList<bool>() << false << true << true << true)
        << (QList<quint32>() << 0 << 2534262748 << 2534262748 << 2786803049);
    QTest::newRow("password-filename.7z")
        << ":/assets/password-filename.7z"
        << "foobar"
        << (QStringList() << "sub" << "1.txt" << "sub/1.txt" << "yellow.png")
        << (QList<quint64>() << 0 << 10 << 10 << 4323)
        << (QList<bool>() << true << false << false << false)
        << (QList<bool>() << false << true << true << true)
        << (QList<quint32>() << 0 << 2534262748 << 2534262748 << 2786803049);
#endif
}

void TestQt7zPackage::extractFile_text()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QString, password);
    QFETCH(QString, fileName);
    QFETCH(QString, fileContent);

    const QString &localPkgPath = extractResource(rcPkgPath);

    Qt7zPackage pkg(localPkgPath);
    TestQt7zPackageClient client;
    pkg.setClient(&client);
    client.password = password;

    QBuffer buf;
    buf.open(QIODevice::ReadWrite);

    QVERIFY(pkg.extractFile(fileName, &buf));
    QString actualFileContent(buf.buffer());
    QCOMPARE(actualFileContent, fileContent);
}

void TestQt7zPackage::extractFile_text_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<QString>("password");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("fileContent");

    QTest::newRow("text.7z/1.txt")
        << ":/assets/text.7z"
        << ""
        << QString("1.txt")
        << QString("I am one.\n");
    QTest::newRow("text.7z/2.txt")
        << ":/assets/text.7z"
        << ""
        << QString("2.txt")
        << QString("I am two.\n");
    QTest::newRow("text.7z/sub/1.txt")
        << ":/assets/text.7z"
        << ""
        << QString("sub/1.txt")
        << QString("I am one.\n");
#ifdef Q_OS_WIN
    QTest::newRow("password.7z/sub/1.txt")
        << ":/assets/password.7z"
        << "foobar"
        << QString("sub/1.txt")
        << QString("I am one.\n");
    QTest::newRow("password-filename.7z/sub/1.txt")
        << ":/assets/password-filename.7z"
        << "foobar"
        << QString("sub/1.txt")
        << QString("I am one.\n");
#endif
}

void TestQt7zPackage::extractFile_image()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QString, password);
    QFETCH(QString, fileName);
    QFETCH(int, width);
    QFETCH(int, height);

    const QString &localPkgPath = extractResource(rcPkgPath);

    Qt7zPackage pkg(localPkgPath);
    TestQt7zPackageClient client;
    pkg.setClient(&client);
    client.password = password;

    QBuffer buf;
    buf.open(QIODevice::ReadWrite);

    QVERIFY(pkg.extractFile(fileName, &buf));
    QImage image = QImage::fromData(buf.buffer());
    QCOMPARE(image.width(), width);
    QCOMPARE(image.height(), height);
}

void TestQt7zPackage::extractFile_image_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<QString>("password");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");

    QTest::newRow("image.7z/red.jpg")
        << ":/assets/image.7z"
        << ""
        << QString("red.jpg")
        << 225
        << 225;
    QTest::newRow("image.7z/yellow.png")
        << ":/assets/image.7z"
        << ""
        << QString("yellow.png")
        << 800
        << 800;
    QTest::newRow("encoding")
        << ":/assets/image.7z"
        << ""
        << QString("[rootnuko＋H] てにおはっ！ ～女の子だってホントはえっちだよ？～/red.jpg")
        << 225
        << 225;
#ifdef Q_OS_WIN
    QTest::newRow("password.7z/yellow.png")
        << ":/assets/password.7z"
        << "foobar"
        << QString("yellow.png")
        << 800
        << 800;
    QTest::newRow("password-filename.7z/yellow.png")
        << ":/assets/password-filename.7z"
        << "foobar"
        << QString("yellow.png")
        << 800
        << 800;
#endif
 }

void TestQt7zPackage::extractFile_wrongPassword()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QString, fileName);

    const QString &localPkgPath = extractResource(rcPkgPath);

    Qt7zPackage pkg(localPkgPath);

    QBuffer buf;
    buf.open(QIODevice::ReadWrite);

    QVERIFY(!pkg.extractFile(fileName, &buf));
}

void TestQt7zPackage::extractFile_wrongPassword_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<QString>("fileName");

    QTest::newRow("password.7z")
        << ":/assets/password.7z"
        << "1.txt";
    QTest::newRow("password-filename.7z")
        << ":/assets/password-filename.7z"
        << "1.txt";
}

QTEST_MAIN(TestQt7zPackage)
#include "Qt7zPackage_Tests.moc"
