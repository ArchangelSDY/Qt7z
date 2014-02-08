#ifndef QT7ZPACKAGE_H
#define QT7ZPACKAGE_H

#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zVersion.h"

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

    // For 7z
    CFileInStream m_archiveStream;
    CLookToRead m_lookStream;
    CSzArEx m_db;
    ISzAlloc m_allocImp;
    ISzAlloc m_allocTempImp;
};

#endif // QT7ZPACKAGE_H
