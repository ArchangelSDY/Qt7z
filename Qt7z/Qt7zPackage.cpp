#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "7zVersion.h"

#include "Qt7zPackage.h"

#include <QDebug>

Qt7zPackage::Qt7zPackage(const QString &packagePath) :
    m_packagePath(packagePath) ,
    m_isOpen(false)
{
}

void Qt7zPackage::reset()
{
    m_packagePath.clear();
    m_isOpen = false;
    m_fileNameList.clear();
}

bool Qt7zPackage::open()
{
    CFileInStream archiveStream;
    CLookToRead lookStream;
    CSzArEx db;
    SRes res;
    ISzAlloc allocImp;
    ISzAlloc allocTempImp;
    UInt16 *temp = NULL;
    size_t tempSize = 0;

    allocImp.Alloc = SzAlloc;
    allocImp.Free = SzFree;

    allocTempImp.Alloc = SzAllocTemp;
    allocTempImp.Free = SzFreeTemp;

    if (InFile_Open(&archiveStream.file, m_packagePath.toUtf8().data())) {
        qDebug() << "Can not open file: " << m_packagePath;
        m_isOpen = false;
        return false;
    }

    FileInStream_CreateVTable(&archiveStream);
    LookToRead_CreateVTable(&lookStream, False);
    
    lookStream.realStream = &archiveStream.s;
    LookToRead_Init(&lookStream);

    CrcGenerateTable();

    SzArEx_Init(&db);
    res = SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp);

    if (res == SZ_OK) {
        UInt32 i;

        for (i = 0; i < db.db.NumFiles; i++) {
            size_t len;

            len = SzArEx_GetFileNameUtf16(&db, i, NULL);

            if (len > tempSize) {
                SzFree(NULL, temp);
                tempSize = len;
                temp = (UInt16 *)SzAlloc(NULL, tempSize * sizeof(temp[0]));
                if (temp == 0) {
                    res = SZ_ERROR_MEM;
                    break;
                }
            }

            SzArEx_GetFileNameUtf16(&db, i, temp);

            // TODO: Codec?
            QString fileName = QString::fromUtf16(temp);
            m_fileNameList << fileName;

            if (res != SZ_OK)
                break;
            continue;
        }
    }

    // TODO: should close in close().
    SzArEx_Free(&db, &allocImp);
    SzFree(NULL, temp);

    File_Close(&archiveStream.file);
    if (res == SZ_OK) {
        // Everything is OK
        m_isOpen = true;
        return true;
    } else {
        m_isOpen = false;
        return false;
    }
}

void Qt7zPackage::close()
{

}

bool Qt7zPackage::isOpen() const
{
    return m_isOpen;
}

QStringList Qt7zPackage::getFileNameList() const
{
    return m_fileNameList;
}
