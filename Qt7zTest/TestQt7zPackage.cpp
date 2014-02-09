#include <QDir>
#include <QDebug>
#include <QImage>
#include <QStringList>
#include <QTest>

#include "../Qt7z/Qt7zPackage.h"

#include "TestQt7zPackage.h"

void TestQt7zPackage::openAndClose()
{
    QFETCH(QString, packagePath);
    QFETCH(bool, valid);

    Qt7zPackage pkg(packagePath);
    QCOMPARE(pkg.open(), valid);
    QCOMPARE(pkg.isOpen(), valid);
    pkg.close();
    QVERIFY(!pkg.isOpen());
}

void TestQt7zPackage::openAndClose_data()
{
    QTest::addColumn<QString>("packagePath");
    QTest::addColumn<bool>("valid");

    QTest::newRow("text.7z")
        << qApp->applicationDirPath() + QDir::separator() + "assets/text.7z"
        << true;
    QTest::newRow("file not exists")
        << "foo"
        << false;
}

void TestQt7zPackage::getFileNameList()
{
    QFETCH(QString, packagePath);
    QFETCH(QStringList, expectFileNameList);

    Qt7zPackage pkg(packagePath);
    QVERIFY(pkg.open());

    const QStringList &actualFileNameList = pkg.getFileNameList();
    QCOMPARE(actualFileNameList.count(), expectFileNameList.count());

    foreach (const QString &actualFileName, actualFileNameList) {
        QVERIFY(expectFileNameList.contains(actualFileName));
    }
}

void TestQt7zPackage::getFileNameList_data()
{
    QTest::addColumn<QString>("packagePath");
    QTest::addColumn<QStringList>("expectFileNameList");

    QTest::newRow("text.7z")
        << qApp->applicationDirPath() + QDir::separator() + "assets/text.7z"
        << (QStringList() << "1.txt" << "2.txt" << "sub/1.txt");
}

void TestQt7zPackage::extractFile_text()
{
    QFETCH(QString, packagePath);
    QFETCH(QString, fileName);
    QFETCH(QString, fileContent);

    Qt7zPackage pkg(packagePath);
    QBuffer buf;
    buf.open(QIODevice::ReadWrite);

    QVERIFY(pkg.extractFile(fileName, &buf));
    QString actualFileContent(buf.buffer());
    QCOMPARE(actualFileContent, fileContent);
}

void TestQt7zPackage::extractFile_text_data()
{
    QTest::addColumn<QString>("packagePath");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("fileContent");

    QTest::newRow("text.7z/1.txt")
        << qApp->applicationDirPath() + QDir::separator() + "assets/text.7z"
        << QString("1.txt")
        << QString("I am one.\n");
    QTest::newRow("text.7z/2.txt")
        << qApp->applicationDirPath() + QDir::separator() + "assets/text.7z"
        << QString("2.txt")
        << QString("I am two.\n");
    QTest::newRow("text.7z/sub/1.txt")
        << qApp->applicationDirPath() + QDir::separator() + "assets/text.7z"
        << QString("sub/1.txt")
        << QString("I am one.\n");
}

void TestQt7zPackage::extractFile_image()
{
    QFETCH(QString, packagePath);
    QFETCH(QString, fileName);
    QFETCH(int, width);
    QFETCH(int, height);

    Qt7zPackage pkg(packagePath);
    QBuffer buf;
    buf.open(QIODevice::ReadWrite);

    QVERIFY(pkg.extractFile(fileName, &buf));
    QImage image = QImage::fromData(buf.buffer());
    QCOMPARE(image.width(), width);
    QCOMPARE(image.height(), height);
}

void TestQt7zPackage::extractFile_image_data()
{
    QTest::addColumn<QString>("packagePath");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");

    QTest::newRow("image.7z/red.jpg")
        << qApp->applicationDirPath() + QDir::separator() + "assets/image.7z"
        << QString("red.jpg")
        << 225
        << 225;
    QTest::newRow("image.7z/yellow.png")
        << qApp->applicationDirPath() + QDir::separator() + "assets/image.7z"
        << QString("yellow.png")
        << 800
        << 800;
    QTest::newRow("encoding")
        << qApp->applicationDirPath() + QDir::separator() + "assets/image.7z"
        << QString("[rootnuko＋H] てにおはっ！ ～女の子だってホントはえっちだよ？～/red.jpg")
        << 225
        << 225;
}
