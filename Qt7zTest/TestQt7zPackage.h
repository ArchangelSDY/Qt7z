#ifndef TESTQT7ZPACKAGE_H
#define TESTQT7ZPACKAGE_H

#include <QObject>

class TestQt7zPackage : public QObject
{
    Q_OBJECT
private slots:
    void open();
    void open_data();
    void getFileNameList();
    void getFileNameList_data();
};

#endif // TESTQT7ZPACKAGE_H
