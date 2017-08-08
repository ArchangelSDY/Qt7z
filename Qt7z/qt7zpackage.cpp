#include "qt7zpackage.h"

#include <QDir>
#include <QScopedPointer>

#ifdef Q_OS_WIN
#include "7zip/CPP/Common/MyWindows.h"
#include "7zip/CPP/Common/MyCom.h"
#include "7zip/CPP/Common/MyInitGuid.h"
#include "7zip/CPP/7zip/Bundles/Alone7z/StdAfx.h"
#include "7zip/CPP/7zip/Common/RegisterArc.h"
#include "7zip/CPP/7zip/Common/RegisterCodec.h"
#include "7zip/CPP/7zip/UI/Common/OpenArchive.h"
#include "7zip/CPP/7zip/UI/Common/Extract.h"
#include "7zip/CPP/Windows/PropVariantConv.h"
#else
#include "p7zip/CPP/include_windows/windows.h"
#include "p7zip/CPP/include_windows/basetyps.h"
#include "p7zip/CPP/include_windows/tchar.h"
#include "p7zip/CPP/myWindows/StdAfx.h"
#include "p7zip/CPP/Common/MyWindows.h"
#include "p7zip/CPP/Common/MyCom.h"
#include "p7zip/CPP/Common/MyInitGuid.h"
#include "p7zip/CPP/7zip/Common/RegisterArc.h"
#include "p7zip/CPP/7zip/Common/RegisterCodec.h"
#include "p7zip/CPP/7zip/UI/Common/OpenArchive.h"
#include "p7zip/CPP/7zip/UI/Common/Extract.h"
#include "p7zip/CPP/Windows/PropVariantConv.h"
#endif

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
    NAMESPACE_FORCE_LINK(Copy)
    NAMESPACE_FORCE_LINK(Lzma)
    NAMESPACE_FORCE_LINK(Lzma2)
    NAMESPACE_FORCE_LINK(Ppmd)
}

namespace NCrypto {
    NAMESPACE_FORCE_LINK(7z)
    NAMESPACE_FORCE_LINK(Aes)
}

extern bool g_forceLinkCRC;

struct ForceLinkCodecs
{
    ForceLinkCodecs()
    {
        bool forceLinkCRC = g_forceLinkCRC;

    #define REFER_TO_FORCE_LINK(NAMESPACE, CODEC) \
        bool forceLink##NAMESPACE##CODEC = N##NAMESPACE::N##CODEC::g_forceLink;

        REFER_TO_FORCE_LINK(Archive, 7z)
        REFER_TO_FORCE_LINK(Compress, Bcj)
        REFER_TO_FORCE_LINK(Compress, Bcj2)
        REFER_TO_FORCE_LINK(Compress, Branch)
        REFER_TO_FORCE_LINK(Compress, Copy)
        REFER_TO_FORCE_LINK(Compress, Lzma)
        REFER_TO_FORCE_LINK(Compress, Lzma2)
        REFER_TO_FORCE_LINK(Compress, Ppmd)
        REFER_TO_FORCE_LINK(Crypto, 7z)
        REFER_TO_FORCE_LINK(Crypto, Aes)
    }

};

#ifdef Q_OS_WIN
__declspec(selectany)ForceLinkCodecs forceLink;
#else
ForceLinkCodecs forceLink;
#endif

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
    OpenCallback(Qt7zPackage::Client *client) :
        m_client(client)
    {
    }

    INTERFACE_IOpenCallbackUI(override;)

private:
    Qt7zPackage::Client *m_client;
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
    if (m_client) {
        QString requestedPassword;
        m_client->openPasswordRequired(requestedPassword);
        std::wstring wpassword = requestedPassword.toStdWString();
        *password = SysAllocStringLen(wpassword.data(), wpassword.size());
    }
    return S_OK;
}

class ExtractCallback : public IArchiveExtractCallback, public ICryptoGetTextPassword, public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP2(IArchiveExtractCallback, ICryptoGetTextPassword)
    INTERFACE_IArchiveExtractCallback(;)
    STDMETHOD(CryptoGetTextPassword)(BSTR *password) override;

    ExtractCallback(Qt7zPackage::Client *client, const Qt7zFileInfo fileInfo, QIODevice *outStream) :
        m_client(client),
        m_fileInfo(fileInfo),
        m_outStream(outStream),
        m_opRes(NArchive::NExtract::NOperationResult::kOK)
    {
    }

    int opRes() const
    {
        return m_opRes;
    }

private:
    Qt7zPackage::Client *m_client;
    Qt7zFileInfo m_fileInfo;
    QIODevice *m_outStream;
    int m_opRes;
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
    m_opRes = opRes;

    switch (opRes)
    {
      case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
        qWarning() << "Qt7z: Extract operation result: Unsupported method";
        break;
      case NArchive::NExtract::NOperationResult::kCRCError:
        if (m_fileInfo.isEncrypted) {
            qWarning() << "Qt7z: Extract operation result: CRC failed in encrypted file. Wrong password?";
        } else {
            qWarning() << "Qt7z: Extract operation result: CRC failed";
        }
        break;
      case NArchive::NExtract::NOperationResult::kDataError:
        if (m_fileInfo.isEncrypted) {
            qWarning() << "Qt7z: Extract operation result: Data error in encrypted file. Wrong password?";
        } else {
            qWarning() << "Qt7z: Extract operation result: Data error";
        }
        break;
      case NArchive::NExtract::NOperationResult::kUnavailable:
        qWarning() << "Qt7z: Extract operation result: Unavailable data";
        break;
      case NArchive::NExtract::NOperationResult::kUnexpectedEnd:
        qWarning() << "Qt7z: Extract operation result: Unexpected end of data";
        break;
      case NArchive::NExtract::NOperationResult::kDataAfterEnd:
        qWarning() << "Qt7z: Extract operation result: There are some data after the end of the payload data";
        break;
      case NArchive::NExtract::NOperationResult::kIsNotArc:
        qWarning() << "Qt7z: Extract operation result: Is not archive";
        break;
      case NArchive::NExtract::NOperationResult::kHeadersError:
        qWarning() << "Qt7z: Extract operation result: Headers error";
        break;
      case NArchive::NExtract::NOperationResult::kWrongPassword:
        qWarning() << "Qt7z: Extract operation result: Wrong password";
        break;
    }

    return S_OK;
}

HRESULT ExtractCallback::CryptoGetTextPassword(BSTR *password)
{
    if (m_client) {
        QString requestedPassword;
        m_client->openPasswordRequired(requestedPassword);
        std::wstring wpassword = requestedPassword.toStdWString();
        *password = SysAllocStringLen(wpassword.data(), wpassword.size());
    }
    return S_OK;
}

class Qt7zPackagePrivate
{
    friend class Qt7zPackage;
public:
    Qt7zPackagePrivate(Qt7zPackage *q);
    Qt7zPackagePrivate(Qt7zPackage *q, const QString &packagePath);

private:
    void init();
    void reset();

    Qt7zPackage *m_q;
    QString m_packagePath;
    Qt7zPackage::Client *m_client;
    bool m_isOpen;
    QStringList m_fileNameList;
    QHash<QString, UInt32> m_fileNameToIndex;
    QList<Qt7zFileInfo> m_fileInfoList;

    QScopedPointer<CCodecs> m_codecs;
    CArchiveLink m_arcLink;
};

Qt7zPackagePrivate::Qt7zPackagePrivate(Qt7zPackage *q) :
    m_q(q) ,
    m_client(nullptr) ,
    m_isOpen(false)
{
    init();
}

Qt7zPackagePrivate::Qt7zPackagePrivate(Qt7zPackage *q,
                                       const QString &packagePath) :
    m_q(q) ,
    m_packagePath(packagePath) ,
    m_client(nullptr) ,
    m_isOpen(false)
{
    init();
}

void Qt7zPackagePrivate::init()
{
    m_codecs.reset(new CCodecs);
    if (m_codecs->Load() != S_OK) {
        qWarning() << "Qt7z: Failed to load codecs";
    }
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

    if (!QFile::exists(m_p->m_packagePath)) {
        qWarning() << "Qt7z: File does not exist:" << m_p->m_packagePath;
        return false;
    }

    m_p->m_arcLink.NonOpen_ErrorInfo.ClearErrors_Full();

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

    OpenCallback callback(m_p->m_client);

    HRESULT res;
    try {
#ifdef Q_OS_WIN
        res = m_p->m_arcLink.Open_Strict(options, &callback);
#else
        res = m_p->m_arcLink.Open3(options, &callback);
#endif
    }
    catch (...) {
        qWarning() << "Qt7z: Exception caught when opening archive";
        return false;
    }

    if (res != S_OK) {
        qWarning() << "Qt7z: Fail to open archive with result" << res;
        return false;
    }

    // TODO: Process error info?
    // for (unsigned i = 0; i < arcLink.Size(); i++) {
    //     const CArcErrorInfo &arc = arcLink.Arcs[i].ErrorInfo;
    // }

    const CArc &arc = m_p->m_arcLink.Arcs.Back();
    IInArchive *archive = arc.Archive;

    UInt32 numItems;
    if (archive->GetNumberOfItems(&numItems) != S_OK) {
        qWarning() << "Qt7z: Fail to get number of items";
        return false;
    }

    CReadArcItem item;
    UStringVector pathParts;

    for (UInt32 i = 0; i < numItems; i++) {
        UString filePath;
        if (arc.GetItemPath2(i, filePath) != S_OK) {
            qWarning() << "Qt7z: Fail to get file path of index" << i;
            return false;
        }

        QString fileName = QDir::fromNativeSeparators(
            QString::fromWCharArray(filePath.Ptr(), filePath.Len()));
        m_p->m_fileNameList << fileName;

        Qt7zFileInfo fileInfo;
        fileInfo.fileName = fileName;
        fileInfo.arcName = m_p->m_packagePath;

        if (Archive_IsItem_Dir(archive, i, fileInfo.isDir) != S_OK) {
            qWarning() << "Qt7z: Fail to determine if directory of index" << i;
            return false;
        }

        CListUInt64Def sizeDef;
        if (GetUInt64Value(archive, i, kpidSize, sizeDef) != S_OK) {
            qWarning() << "Qt7z: Fail to file size of index" << i;
            return false;
        }
        fileInfo.size = sizeDef.Val;

        CListUInt64Def crcDef;
        if (GetUInt64Value(archive, i, kpidCRC, crcDef) != S_OK) {
            qWarning() << "Qt7z: Fail to CRC of index" << i;
            return false;
        }
        fileInfo.isCrcDefined = crcDef.Def;
        fileInfo.crc = crcDef.Val;

        if (Archive_GetItemBoolProp(archive, i, kpidEncrypted, fileInfo.isEncrypted) != S_OK) {
            qWarning() << "Qt7z: Fail to determine if encrypted of index" << i;
            return false;
        }

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

Qt7zPackage::Client *Qt7zPackage::client() const
{
    return m_p->m_client;
}

void Qt7zPackage::setClient(Qt7zPackage::Client *client)
{
    m_p->m_client = client;
}

bool Qt7zPackage::extractFile(const QString &name, QIODevice *outStream)
{
    if (!outStream || !outStream->isWritable()) {
        qWarning() << "Qt7z: Extract output stream is null or not writable!";
        return false;
    }

    if (!m_p->m_isOpen) {
        if (!open()) {
            qWarning() << "Qt7z: Fail to extract file" << name << "due to unable to open archive" << m_p->m_packagePath;
            return false;
        }
    }

    auto indexIt = m_p->m_fileNameToIndex.find(name);
    if (indexIt == m_p->m_fileNameToIndex.end()) {
        qWarning() << "Qt7z: Fail to find file" << name;
        return false;
    }
    UInt32 index = *indexIt;
    Qt7zFileInfo fileInfo = m_p->m_fileInfoList[index];

    const CArc &arc = m_p->m_arcLink.Arcs.Back();
    IInArchive *archive = arc.Archive;
    CRecordVector<UInt32> realIndices;
    realIndices.Add(index);
    Int32 testMode = 0;

    CMyComPtr<ExtractCallback> callback(new ExtractCallback(m_p->m_client, fileInfo, outStream));
    HRESULT res = archive->Extract(&realIndices.Front(), realIndices.Size(), testMode, callback);

    if (res != S_OK) {
        qWarning() << "Qt7z: Fail to extract file" << name << "with result" << res;
        return false;
    }
    if (callback->opRes() != NArchive::NExtract::NOperationResult::kOK) {
        qWarning() << "Qt7z: Fail to extract file" << name << "with op res" << callback->opRes();
        return false;
    }

    return true;
}
