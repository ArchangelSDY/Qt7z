#include <QDir>
#include <QStringList>
#include <QTest>

#include "../Qt7z/Qt7zPackage.h"

#include "TestQt7zPackage.h"

void TestQt7zPackage::open()
{
    QFETCH(QString, packagePath);

    Qt7zPackage pkg(packagePath);
    QVERIFY(pkg.open());
}

void TestQt7zPackage::open_data()
{
    QTest::addColumn<QString>("packagePath");

    QTest::newRow("test1.7z")
        << qApp->applicationDirPath() + QDir::separator() + "assets/test1.7z";
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

    QTest::newRow("test1.7z")
        << qApp->applicationDirPath() + QDir::separator() + "assets/test1.7z"
        << (QStringList() << "1.txt" << "2.txt");
}
