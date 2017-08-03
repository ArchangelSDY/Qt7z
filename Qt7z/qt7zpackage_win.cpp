#include "qt7zpackage.h"

#include <QDir>
#include <QScopedPointer>

#include "7zip/CPP/Common/MyWindows.h"
#include "7zip/CPP/Common/MyCom.h"
#include "7zip/CPP/Common/MyInitGuid.h"
#include "7zip/CPP/7zip/Bundles/Alone7z/StdAfx.h"
#include "7zip/CPP/7zip/Common/RegisterArc.h"
#include "7zip/CPP/7zip/Common/RegisterCodec.h"
#include "7zip/CPP/7zip/UI/Common/OpenArchive.h"
#include "7zip/CPP/7zip/UI/Common/Extract.h"
#include "7zip/CPP/Windows/PropVariantConv.h"

#include "qt7zfileinfo.h"

#include <QDebug>

using namespace NWindows;
using namespace NFile;

#define NAMESPACE_FORCE_LINK(CODEC) \
    namespace N##CODEC { \
        extern bool g_forceLink; \
    }

namespace NArchive {
    NAMESPACE_FORCE_LINK(7z)
}

namespace NCompress {
    NAMESPACE_FORCE_LINK(Bcj)
    NAMESPACE_FORCE_LINK(Bcj2)
    NAMESPACE_FORCE_LINK(Branch)
    NAMESPACE_FORCE_LINK(BZip2)
    NAMESPACE_FORCE_LINK(Copy)
    NAMESPACE_FORCE_LINK(Deflate)
    NAMESPACE_FORCE_LINK(Deflate64)
    NAMESPACE_FORCE_LINK(Lzma)
    NAMESPACE_FORCE_LINK(Lzma2)
    NAMESPACE_FORCE_LINK(Ppmd)
    NAMESPACE_FORCE_LINK(RAR)
}

extern bool g_forceLinkCRC;

struct CListUInt64Def
{
  UInt64 Val;
  bool Def;

  CListUInt64Def(): Val(0), Def(false) {}
  void Add(UInt64 v) { Val += v; Def = true; }
  void Add(const CListUInt64Def &v) { if (v.Def) Add(v.Val); }
};

static HRESULT GetUInt64Value(IInArchive *archive, UInt32 index, PROPID propID, CListUInt64Def &value)
{
  value.Val = 0;
  value.Def = false;
  NWindows::NCOM::CPropVariant prop;
  RINOK(archive->GetProperty(index, propID, &prop));
  value.Def = ConvertPropVariantToUInt64(prop, value.Val);
  return S_OK;
}

class SequentialStreamAdapter : public ISequentialOutStream, public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP1(ISequentialOutStream)
    STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);

    SequentialStreamAdapter(QIODevice *device) : CMyUnknownImp(), m_device(device) {}

private:
    QIODevice *m_device;
};

HRESULT SequentialStreamAdapter::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
    *processedSize = m_device->write(reinterpret_cast<const char *>(data), size);
    return S_OK;
}

class OpenCallback : public IOpenCallbackUI, public CMyUnknownImp
{
public:
    INTERFACE_IOpenCallbackUI(override;)
};

HRESULT OpenCallback::Open_CheckBreak()
{
    return S_OK;
}

HRESULT OpenCallback::Open_SetTotal(const UInt64 *files, const UInt64 *bytes)
{
    return S_OK;
}
 
HRESULT OpenCallback::Open_SetCompleted(const UInt64 *files, const UInt64 *bytes)
{
    return S_OK;
}

HRESULT OpenCallback::Open_Finished()
{
    return S_OK;
}

HRESULT OpenCallback::Open_CryptoGetTextPassword(BSTR *password)
{
    return S_OK;
}

class ExtractCallback : public IArchiveExtractCallback, public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP1(IArchiveExtractCallback)
    INTERFACE_IArchiveExtractCallback(;)

    ExtractCallback(QIODevice *outStream) :
        m_outStream(outStream) {}

private:
    QIODevice *m_outStream;
};

HRESULT ExtractCallback::SetTotal(UInt64 total)
{
    return S_OK;
}

HRESULT ExtractCallback::SetCompleted(const UInt64 *completeValue)
{
    return S_OK;
}

HRESULT ExtractCallback::GetStream(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode)
{
    if (askExtractMode != NArchive::NExtract::NAskMode::kExtract) {
        *outStream = nullptr;
        return S_OK;
    }

    CMyComPtr<ISequentialOutStream> stream(new SequentialStreamAdapter(m_outStream));
    *outStream = stream.Detach();
    return S_OK;
}

HRESULT ExtractCallback::PrepareOperation(Int32 askExtractMode)
{
    return S_OK;
}

HRESULT ExtractCallback::SetOperationResult(Int32 opRes)
{
    return S_OK;
}

class Qt7zPackagePrivate
{
    friend class Qt7zPackage;
public:
    Qt7zPackagePrivate(Qt7zPackage *q);
    Qt7zPackagePrivate(Qt7zPackage *q, const QString &packagePath);

private:
    static void forceLinkCodecs();
    void init();
    void reset();

    Qt7zPackage *m_q;
    QString m_packagePath;
    bool m_isOpen;
    QStringList m_fileNameList;
    QHash<QString, UInt32> m_fileNameToIndex;
    QList<Qt7zFileInfo> m_fileInfoList;

    QScopedPointer<CCodecs> m_codecs;
    CArchiveLink m_arcLink;
};

Qt7zPackagePrivate::Qt7zPackagePrivate(Qt7zPackage *q) :
    m_q(q) ,
    m_isOpen(false)
{
    init();
}

Qt7zPackagePrivate::Qt7zPackagePrivate(Qt7zPackage *q,
                                       const QString &packagePath) :
    m_q(q) ,
    m_packagePath(packagePath) ,
    m_isOpen(false)
{
    init();
}

void Qt7zPackagePrivate::init()
{
    m_codecs.reset(new CCodecs);
    if (m_codecs->Load() != S_OK) {
        qDebug() << "Qt7z: Failed to load codecs";
    }
}

void Qt7zPackagePrivate::forceLinkCodecs()
{
    bool forceLinkCRC = g_forceLinkCRC;

#define REFER_TO_FORCE_LINK(NAMESPACE, CODEC) \
    bool forceLink##CODEC = N##NAMESPACE::N##CODEC::g_forceLink;

    REFER_TO_FORCE_LINK(Archive, 7z)
    REFER_TO_FORCE_LINK(Compress, Bcj)
    REFER_TO_FORCE_LINK(Compress, Bcj2)
    REFER_TO_FORCE_LINK(Compress, Branch)
    REFER_TO_FORCE_LINK(Compress, BZip2)
    REFER_TO_FORCE_LINK(Compress, Copy)
    REFER_TO_FORCE_LINK(Compress, Deflate)
    REFER_TO_FORCE_LINK(Compress, Deflate64)
    REFER_TO_FORCE_LINK(Compress, Lzma)
    REFER_TO_FORCE_LINK(Compress, Lzma2)
    REFER_TO_FORCE_LINK(Compress, Ppmd)
    REFER_TO_FORCE_LINK(Compress, RAR)
}

void Qt7zPackagePrivate::reset()
{
    m_packagePath.clear();
    m_isOpen = false;
    m_fileNameList.clear();
    m_fileInfoList.clear();
}

Qt7zPackage::Qt7zPackage() :
    m_p(new Qt7zPackagePrivate(this))
{
}

Qt7zPackage::Qt7zPackage(const QString &packagePath) :
    m_p(new Qt7zPackagePrivate(this, packagePath))
{
}

Qt7zPackage::~Qt7zPackage()
{
    close();
    if (m_p) {
        delete m_p;
    }
}

bool Qt7zPackage::open()
{
    if (m_p->m_isOpen) {
        return false;
    }

    // TODO: Set password callback

    CObjectVector<CProperty> props;
    CObjectVector<COpenType> types;   // TODO: ok to leave blank?
    CIntVector excludedFormats;

    COpenOptions options;
    options.props = &props;
    options.codecs = m_p->m_codecs.data();
    options.types = &types;
    options.excludedFormats = &excludedFormats;
    options.stdInMode = false;
    options.stream = nullptr;

    std::wstring packagePathW = m_p->m_packagePath.toStdWString();
    options.filePath = UString(packagePathW.data());

    OpenCallback callback;

    HRESULT res;
    try {
        res = m_p->m_arcLink.Open_Strict(options, &callback);
    }
    catch (...) {
        qDebug() << "Qt7z: Exception caught when opening archive";
        return false;
    }

    if (res != S_OK) {
        qDebug() << "Qt7z: Fail to open archive with result" << res;
        return false;
    }


    // TODO: Process error info?
    // for (unsigned i = 0; i < arcLink.Size(); i++) {
    //     const CArcErrorInfo &arc = arcLink.Arcs[i].ErrorInfo;
    //     if ()
    // }

    const CArc &arc = m_p->m_arcLink.Arcs.Back();
    IInArchive *archive = arc.Archive;

    UInt32 numItems;
    if (archive->GetNumberOfItems(&numItems) != S_OK) {
        qDebug() << "Qt7z: Fail to get number of items";
        return false;
    }

    CReadArcItem item;
    UStringVector pathParts;

    for (UInt32 i = 0; i < numItems; i++) {
        UString filePath;
        if (arc.GetItemPath2(i, filePath) != S_OK) {
            qDebug() << "Qt7z: Fail to get file path of index" << i;
            return false;
        }

        QString fileName = QDir::fromNativeSeparators(
            QString::fromWCharArray(filePath.Ptr(), filePath.Len()));
        m_p->m_fileNameList << fileName;

        Qt7zFileInfo fileInfo;
        fileInfo.fileName = fileName;
        fileInfo.arcName = m_p->m_packagePath;

        if (Archive_IsItem_Dir(archive, i, fileInfo.isDir) != S_OK) {
            qDebug() << "Qt7z: Fail to determine if directory of index" << i;
            return false;
        }

        CListUInt64Def sizeDef;
        if (GetUInt64Value(archive, i, kpidSize, sizeDef) != S_OK) {
            qDebug() << "Qt7z: Fail to file size of index" << i;
            return false;
        }
        fileInfo.size = sizeDef.Val;

        CListUInt64Def crcDef;
        if (GetUInt64Value(archive, i, kpidCRC, crcDef) != S_OK) {
            qDebug() << "Qt7z: Fail to CRC of index" << i;
            return false;
        }
        fileInfo.isCrcDefined = crcDef.Def;
        fileInfo.crc = crcDef.Val;

        m_p->m_fileInfoList << fileInfo;
        m_p->m_fileNameToIndex.insert(fileName, i);
    }

    m_p->m_isOpen = true;

    return true;
}

void Qt7zPackage::close()
{
    if (m_p->m_isOpen) {
        m_p->m_isOpen = false;
    }
}

bool Qt7zPackage::isOpen() const
{
    return m_p->m_isOpen;
}

QStringList Qt7zPackage::getFileNameList() const
{
    return m_p->m_fileNameList;
}

QStringList Qt7zPackage::fileNameList() const
{
    return m_p->m_fileNameList;
}

QList<Qt7zFileInfo> &Qt7zPackage::fileInfoList() const
{
    return m_p->m_fileInfoList;
}

bool Qt7zPackage::extractFile(const QString &name, QIODevice *outStream)
{
    if (!outStream || !outStream->isWritable()) {
        qDebug() << "Qt7z: Extract output stream is null or not writable!";
        return false;
    }

    if (!m_p->m_isOpen) {
        if (!open()) {
            qDebug() << "Qt7z: Cannot open package for extracting!";
            return false;
        }
    }

    auto indexIt = m_p->m_fileNameToIndex.find(name);
    if (indexIt == m_p->m_fileNameToIndex.end()) {
        qDebug() << "Qt7z: Fail to find file" << name;
        return false;
    }
    UInt32 index = *indexIt;

    const CArc &arc = m_p->m_arcLink.Arcs.Back();
    IInArchive *archive = arc.Archive;
    CRecordVector<UInt32> realIndices;
    realIndices.Add(index);
    Int32 testMode = 0;

    CMyComPtr<ExtractCallback> callback(new ExtractCallback(outStream));
    HRESULT res = archive->Extract(&realIndices.Front(), realIndices.Size(), testMode, callback);
    if (res != S_OK) {
        qDebug() << "Qt7z: Fail to extract file" << name << "with result" << res;
    }
    return res == S_OK;
}