#ifndef TESTQT7ZPACKAGE_H
#define TESTQT7ZPACKAGE_H

#include <QObject>

class TestQt7zPackage : public QObject
{
    Q_OBJECT
private slots:
    void openAndClose();
    void openAndClose_data();
    void getFileNameList();
    void getFileNameList_data();
    void extractFile();
    void extractFile_data();
};

#endif // TESTQT7ZPACKAGE_H
