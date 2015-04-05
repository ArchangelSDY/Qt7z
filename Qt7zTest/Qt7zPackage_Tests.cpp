#include <QDir>
#include <QImage>
#include <QScopedPointer>
#include <QStringList>
#include <QTemporaryFile>
#include <QTest>

#include "../Qt7z/Qt7zPackage.h"

class TestQt7zPackage : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();

    void openAndClose();
    void openAndClose_data();
    void getFileNameList();
    void getFileNameList_data();
    void extractFile_text();
    void extractFile_text_data();
    void extractFile_image();
    void extractFile_image_data();

private:
    QFile *extractResource(const QString &path);
};

void TestQt7zPackage::initTestCase()
{
    Q_INIT_RESOURCE(Qt7zTest);
}

QFile *TestQt7zPackage::extractResource(const QString &path)
{
    QFile r(path);
    if (!r.open(QIODevice::ReadOnly)) {
        return 0;
    }

    QTemporaryFile *f = new QTemporaryFile();
    if (!f->open()) {
        delete f;
        return 0;
    }

    f->write(r.readAll());
    f->close();

    return f;
}

void TestQt7zPackage::openAndClose()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(bool, valid);

    QScopedPointer<QFile> localPkg(extractResource(rcPkgPath));
    QString localPkgPath = localPkg.isNull()
        ? rcPkgPath
        : localPkg->fileName();

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
    QTest::newRow("file not exists")
        << "foo"
        << false;
}

void TestQt7zPackage::getFileNameList()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QStringList, expectFileNameList);

    QScopedPointer<QFile> localPkg(extractResource(rcPkgPath));
    QString localPkgPath = localPkg.isNull()
        ? rcPkgPath
        : localPkg->fileName();

    Qt7zPackage pkg(localPkgPath);

    QVERIFY(pkg.open());

    const QStringList &actualFileNameList = pkg.getFileNameList();
    QCOMPARE(actualFileNameList.count(), expectFileNameList.count());

    foreach (const QString &actualFileName, actualFileNameList) {
        QVERIFY(expectFileNameList.contains(actualFileName));
    }
}

void TestQt7zPackage::getFileNameList_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<QStringList>("expectFileNameList");

    QTest::newRow("text.7z")
        << ":/assets/text.7z"
        << (QStringList() << "1.txt" << "2.txt" << "sub/1.txt");
}

void TestQt7zPackage::extractFile_text()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QString, fileName);
    QFETCH(QString, fileContent);

    QScopedPointer<QFile> localPkg(extractResource(rcPkgPath));
    QString localPkgPath = localPkg.isNull()
        ? rcPkgPath
        : localPkg->fileName();

    Qt7zPackage pkg(localPkgPath);

    QBuffer buf;
    buf.open(QIODevice::ReadWrite);

    QVERIFY(pkg.extractFile(fileName, &buf));
    QString actualFileContent(buf.buffer());
    QCOMPARE(actualFileContent, fileContent);
}

void TestQt7zPackage::extractFile_text_data()
{
    QTest::addColumn<QString>("rcPkgPath");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("fileContent");

    QTest::newRow("text.7z/1.txt")
        << ":/assets/text.7z"
        << QString("1.txt")
        << QString("I am one.\n");
    QTest::newRow("text.7z/2.txt")
        << ":/assets/text.7z"
        << QString("2.txt")
        << QString("I am two.\n");
    QTest::newRow("text.7z/sub/1.txt")
        << ":/assets/text.7z"
        << QString("sub/1.txt")
        << QString("I am one.\n");
}

void TestQt7zPackage::extractFile_image()
{
    QFETCH(QString, rcPkgPath);
    QFETCH(QString, fileName);
    QFETCH(int, width);
    QFETCH(int, height);

    QScopedPointer<QFile> localPkg(extractResource(rcPkgPath));
    QString localPkgPath = localPkg.isNull()
        ? rcPkgPath
        : localPkg->fileName();

    Qt7zPackage pkg(localPkgPath);

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
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");

    QTest::newRow("image.7z/red.jpg")
        << ":/assets/image.7z"
        << QString("red.jpg")
        << 225
        << 225;
    QTest::newRow("image.7z/yellow.png")
        << ":/assets/image.7z"
        << QString("yellow.png")
        << 800
        << 800;
    QTest::newRow("encoding")
        << ":/assets/image.7z"
        << QString("[rootnuko＋H] てにおはっ！ ～女の子だってホントはえっちだよ？～/red.jpg")
        << 225
        << 225;
}

QTEST_MAIN(TestQt7zPackage)
#include "Qt7zPackage_Tests.moc"
