#include "Qt7zPackage.h"

#include <QDebug>

Qt7zPackage::Qt7zPackage(const QString &packagePath) :
    m_packagePath(packagePath) ,
    m_isOpen(false) ,
    m_blockIndex(0xFFFFFFFF) ,
    m_outBuffer(0) ,
    m_outBufferSize(0)
{
    m_allocImp.Alloc = SzAlloc;
    m_allocImp.Free = SzFree;

    m_allocTempImp.Alloc = SzAllocTemp;
    m_allocTempImp.Free = SzFreeTemp;
}

Qt7zPackage::~Qt7zPackage()
{
    if (m_outBuffer) {
        SzFree(0, m_outBuffer);
    }
}

void Qt7zPackage::reset()
{
    m_packagePath.clear();
    m_isOpen = false;
    m_fileNameList.clear();

    m_blockIndex = 0xFFFFFFFF;
    if (m_outBuffer) {
        SzFree(0, m_outBuffer);
        m_outBuffer = 0;
    }
    m_outBufferSize = 0;
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

bool Qt7zPackage::extractFile(const QString &name, QIODevice *outStream)
{
    if (!outStream || !outStream->isWritable()) {
        qDebug() << "Extract output stream is null or not writable!";
        return false;
    }

    if (!m_isOpen) {
        if (!open()) {
            qDebug() << "Cannot open package for extracting!";
            return false;
        }
    }

    int index = m_fileNameList.indexOf(name);
    if (index == -1) {
        qDebug() << "Cannot find file: " << name;
        return false;
    }

    size_t offset = 0;
    size_t outSizeProcessed = 0;

    SRes res;
    res = SzArEx_Extract(&m_db, &m_lookStream.s, index,
        &m_blockIndex, &m_outBuffer, &m_outBufferSize,
        &offset, &outSizeProcessed, &m_allocImp, &m_allocTempImp);
    if (res != SZ_OK) {
        qDebug() << "Fail to extract" << name;
        return false;
    }

    qint64 writtenSize = outStream->write(
        reinterpret_cast<const char *>(m_outBuffer + offset),
        outSizeProcessed);
    if (static_cast<size_t>(writtenSize) != outSizeProcessed) {
        qDebug() << "Fail to write all extracted data!";
        return false;
    }

    return true;
}
