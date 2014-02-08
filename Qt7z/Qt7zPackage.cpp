#include "Qt7zPackage.h"

#include <QDebug>

Qt7zPackage::Qt7zPackage(const QString &packagePath) :
    m_packagePath(packagePath) ,
    m_isOpen(false)
{
    m_allocImp.Alloc = SzAlloc;
    m_allocImp.Free = SzFree;

    m_allocTempImp.Alloc = SzAllocTemp;
    m_allocTempImp.Free = SzFreeTemp;
}

void Qt7zPackage::reset()
{
    m_packagePath.clear();
    m_isOpen = false;
    m_fileNameList.clear();
}

bool Qt7zPackage::open()
{
    if (m_isOpen) {
        return false;
    }

    SRes res;
    UInt16 *temp = NULL;
    size_t tempSize = 0;

    if (InFile_Open(&m_archiveStream.file, m_packagePath.toUtf8().data())) {
        qDebug() << "Can not open file: " << m_packagePath;
        m_isOpen = false;
        return false;
    }

    FileInStream_CreateVTable(&m_archiveStream);
    LookToRead_CreateVTable(&m_lookStream, False);
    
    m_lookStream.realStream = &m_archiveStream.s;
    LookToRead_Init(&m_lookStream);

    CrcGenerateTable();

    SzArEx_Init(&m_db);
    res = SzArEx_Open(&m_db, &m_lookStream.s, &m_allocImp, &m_allocTempImp);

    if (res == SZ_OK) {
        for (UInt32 i = 0; i < m_db.db.NumFiles; i++) {
            size_t len = SzArEx_GetFileNameUtf16(&m_db, i, NULL);

            if (len > tempSize) {
                SzFree(NULL, temp);
                tempSize = len;
                temp = (UInt16 *)SzAlloc(NULL, tempSize * sizeof(temp[0]));
                if (temp == 0) {
                    res = SZ_ERROR_MEM;
                    break;
                }
            }

            SzArEx_GetFileNameUtf16(&m_db, i, temp);

            // TODO: Codec?
            QString fileName = QString::fromUtf16(temp);
            m_fileNameList << fileName;

            if (res != SZ_OK)
                break;
        }
    }

    SzFree(NULL, temp);

    if (res == SZ_OK) {
        m_isOpen = true;
        return true;
    } else {
        m_isOpen = false;
        return false;
    }
}

void Qt7zPackage::close()
{
    SzArEx_Free(&m_db, &m_allocImp);
    File_Close(&m_archiveStream.file);
    m_isOpen = false;
}

bool Qt7zPackage::isOpen() const
{
    return m_isOpen;
}

QStringList Qt7zPackage::getFileNameList() const
{
    return m_fileNameList;
}
