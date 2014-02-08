#ifndef QT7ZPACKAGE_H
#define QT7ZPACKAGE_H

#include <QString>
#include <QStringList>

class Qt7zPackage
{
public:
    Qt7zPackage(const QString &packagePath);

    bool open();
    void close();

    bool isOpen() const;
    QStringList getFileNameList() const;

private:
    void reset();

    QString m_packagePath;
    bool m_isOpen;
    QStringList m_fileNameList;
};

#endif // QT7ZPACKAGE_H
