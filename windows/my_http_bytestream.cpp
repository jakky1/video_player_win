#include "my_http_bytestream.h"

#include <windows.h>
#include <winhttp.h>
#include <mfobjects.h> // IMFByteStream
#include <mfapi.h> // MFCreateAsyncResult
#include <mferror.h>

#pragma comment(lib, "WinHTTP")
#pragma comment(lib, "Mfuuid") // IMFByteStream
#pragma comment(lib, "Mfplat") // MFCreateAsyncResult

#include <future>
#include <vector>
#include <cassert>

#include <iostream>
#include <fstream>

class MyHttpConnection
{
public:
    bool open(std::wstring& url, std::vector<std::wstring> headers = {}, QWORD startPosition = 0);
    int read(BYTE* buf, int size);
    void close();
    long getContentLength();
    ~MyHttpConnection();

private:
    char buf[1024 * 8];
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
};

#define CHECK(result, errMsg) if (!result) { std::cerr << errMsg << GetLastError() << std::endl; close(); return false; }
bool MyHttpConnection::open(std::wstring& url, std::vector<std::wstring> headers, QWORD startPosition)
{
    BOOL bResults = FALSE;
    URL_COMPONENTS urlComp = { sizeof(URL_COMPONENTS) };
    WCHAR lpszHostName[1024];
    WCHAR lpszUrlPath[1024];

    assert(hSession == NULL);

    urlComp.lpszHostName = lpszHostName;
    urlComp.lpszUrlPath = lpszUrlPath;
    urlComp.dwHostNameLength = sizeof(lpszHostName) / sizeof(WCHAR);
    urlComp.dwUrlPathLength = sizeof(lpszUrlPath) / sizeof(WCHAR);
    CHECK(WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp), "WinHttpCrackUrl failed");

    hSession = WinHttpOpen(L"A WinHTTP Example Program/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    CHECK(hSession, "WinHttpOpen failed");

    hConnect = WinHttpConnect(hSession, urlComp.lpszHostName,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    CHECK(hConnect, "WinHttpConnect failed");

    hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath,
        NULL, WINHTTP_NO_REFERER,
        NULL, WINHTTP_FLAG_SECURE);
    CHECK(hRequest, "WinHttpOpenRequest failed");

    if (startPosition > 0)
    {
        wchar_t rangeHeader[50];
        swprintf(rangeHeader, sizeof(rangeHeader) / sizeof(wchar_t), L"Range: bytes=%d-", (int)startPosition);
        WinHttpAddRequestHeaders(hRequest, rangeHeader, (DWORD) wcslen(rangeHeader), (DWORD) WINHTTP_ADDREQ_FLAG_ADD);
    }

    //std::wcout << L"start pass headers..." << std::endl;
    for (auto header : headers)
    {
        //std::wcout << L"header: " << header << std::endl;
        BOOL b = WinHttpAddRequestHeaders(hRequest, header.c_str(), (DWORD) header.length(), (DWORD) WINHTTP_ADDREQ_FLAG_ADD);
        CHECK(b, "WinHttpAddRequestHeaders failed");
    }

    bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA,
        0, 0, 0);
    CHECK(bResults, "WinHttpSendRequest failed");

    bResults = WinHttpReceiveResponse(hRequest, NULL);
    CHECK(bResults, "WinHttpReceiveResponse failed");

    return true;
}

int MyHttpConnection::read(BYTE* pBuf, int size)
{
    DWORD bytesRead;

    BOOL bResults = WinHttpReadData(hRequest, pBuf, (DWORD) size, &bytesRead);
    if (bResults && bytesRead > 0) {
        return bytesRead;
    }
    return -1;
}

void MyHttpConnection::close()
{
    if (hSession != NULL)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
    }
    hRequest = hConnect = hSession = NULL;
}

long MyHttpConnection::getContentLength()
{
    if (!hRequest) return -1;
    DWORD contentLength = 0;
    DWORD length = sizeof(contentLength);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
        NULL, &contentLength, &length, NULL)) {
        //std::wcout << L"Content Length: " << contentLength << L" bytes" << std::endl;
        return contentLength;
    }

    return -1;
}

MyHttpConnection::~MyHttpConnection()
{
    //std::cout << "MyHttpConnection::~MyHttpConnection()" << std::endl;
    close();
}

// --------------------------------------------------------------------------

class MyDataIUnknown : public IUnknown
{
public:
    MyDataIUnknown(ULONG data) : mData(data) {}
    long getData() { return mData; }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (riid == IID_IUnknown) {
            *ppv = static_cast<MyDataIUnknown*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&refCount);
    }

    STDMETHODIMP_(ULONG) Release() {
        ULONG count = InterlockedDecrement(&refCount);
        if (count == 0) {
            delete this;
        }
        return count;
    }

private:
    ULONG refCount = 1;
    ULONG mData;
};

STDMETHODIMP MyHttpByteStream::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv)
        return E_POINTER;

    if (riid == __uuidof(IUnknown) ||
        riid == __uuidof(IMFByteStream))
    {
        *ppv = static_cast<IMFByteStream*>(this);
    }
    else if (riid == __uuidof(IMFByteStreamBuffering))
    {
        *ppv = static_cast<IMFByteStreamBuffering*>(this);
    }
    /*
    else if (riid == __uuidof(IMFMediaEventGenerator))
    {
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    }
    */
    else
    {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

HRESULT MyHttpByteStream::GetCapabilities(DWORD* pdwCapabilities)
{
    *pdwCapabilities = MFBYTESTREAM_IS_READABLE
        | MFBYTESTREAM_IS_SEEKABLE
        | MFBYTESTREAM_IS_REMOTE
        | MFBYTESTREAM_HAS_SLOW_SEEK;
    return S_OK;
}

HRESULT MyHttpByteStream::GetLength(QWORD* pqwLength)
{
    if (mFileSize > 0) {
        *pqwLength = mFileSize;
        //std::cout << "MyHttpByteStream::GetLength end : size = " << mFileSize << std::endl;
        return S_OK;
    }

    if (!mConnection) openConnection(0);
    if (mConnection)
    {
        mFileSize = mConnection->getContentLength();
        *pqwLength = mFileSize;
        //std::cout << "MyHttpByteStream::GetLength end : size = " << mFileSize << std::endl;
        return S_OK;
    }
    return E_FAIL;
}

HRESULT MyHttpByteStream::IsEndOfStream(BOOL* pfEndOfStream)
{
    QWORD size;
    if (!SUCCEEDED(GetLength(&size))) return E_FAIL;
    *pfEndOfStream = (mPosition >= size);
    return S_OK;
}

bool MyHttpByteStream::openConnection(QWORD position)
{
    if (!mConnection) delete mConnection;

    mConnection = new MyHttpConnection();
    if (!mConnection->open(mUrl, mHeaders, position))
    {
        delete mConnection;
        mConnection = NULL;
        return false;
    }
    mPosition = position;
    return true;
}

HRESULT MyHttpByteStream::GetCurrentPosition(QWORD* pqwPosition)
{
    *pqwPosition = mPosition;
    return S_OK;
}

HRESULT MyHttpByteStream::SetCurrentPosition(QWORD qwPosition)
{
    if (mPosition != qwPosition)
    {
        mPosition = qwPosition;
        if (mConnection)
        {
            delete mConnection;
            mConnection = NULL;
        }
    }
    return S_OK;
}

HRESULT MyHttpByteStream::Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin,
    LONGLONG llSeekOffset,
    DWORD dwSeekFlags,
    QWORD* pqwCurrentPosition)
{
    long pos;
    if (msoBegin == SeekOrigin) pos = (long) llSeekOffset;
    else pos = (long) mPosition + (long) llSeekOffset;
    return SetCurrentPosition(pos);
}

HRESULT MyHttpByteStream::Read(BYTE* pb, ULONG cb, ULONG* pcbRead)
{
    if (!mConnection)
    {
        if (!openConnection(mPosition)) return E_FAIL;
    }

    int totalLen = 0;
    while (cb > 0)
    {
        int len = mConnection->read(pb, cb);
        if (len < 0) break;
        mPosition += len;
        pb += len;
        totalLen += len;
        cb -= len;
        break;
    }
    *pcbRead = totalLen;
    return S_OK; 
}

HRESULT MyHttpByteStream::BeginRead(BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState)
{
    if (!pb || cb == 0 || !pCallback) {
        return E_POINTER;
    }

    auto ret = std::async(std::launch::async, [=]() -> void {
        wil::com_ptr<IMFAsyncResult> pAsyncResult;
        wil::com_ptr<MyDataIUnknown> pData;

        ULONG readLen = 0;
        Read(pb, cb, &readLen);

        pData.attach(new MyDataIUnknown(readLen));
        MFCreateAsyncResult(pData.get(), pCallback, punkState, &pAsyncResult);
        MFInvokeCallback(pAsyncResult.get());
        });
    return S_OK;
}

HRESULT MyHttpByteStream::EndRead(IMFAsyncResult* pResult, ULONG* pcbRead)
{
    if (!pResult || !pcbRead) {
        return E_POINTER;
    }

    wil::com_ptr<MyDataIUnknown> pData;
    pResult->GetObject((IUnknown**)&pData);
    if (!pData) return E_FAIL;
    *pcbRead = pData->getData();
    return S_OK;
}

HRESULT MyHttpByteStream::Close()
{
    //std::cout << "MyHttpByteStream::Close" << std::endl;
    if (mConnection)
    {
        delete mConnection;
        mConnection = NULL;

        //m_eventQueue->Shutdown();
        //m_eventQueue.reset(); // Release() will be called

        return S_OK;
    }
    return E_FAIL;
}

MyHttpByteStream::MyHttpByteStream(const std::wstring& url, const std::vector<std::wstring> headers)
{
    std::cout << "MyHttpByteStream::MyHttpByteStream()" << std::endl;
    mUrl = url;
    mHeaders = headers;
    //MFCreateEventQueue(&m_eventQueue);
}

MyHttpByteStream::~MyHttpByteStream()
{
    std::cout << "MyHttpByteStream::~MyHttpByteStream()" << std::endl;
    Close();
}

// --------------------------------------------------------------------------
// IMFByteStreamBuffering implementation
// --------------------------------------------------------------------------

HRESULT MyHttpByteStream::EnableBuffering(BOOL fEnable)
{
    // NOTE: without this implementation, buffering event notification still works... why ?

    /*
    std::cout << "MyHttpByteStream::EnableBuffering start" << std::endl;
    std::lock_guard<std::mutex> lock(m_mutex);

    if (fEnable)
    {
        if (!m_bufferingEnabled)
        {
            m_bufferingEnabled = true;
            StartBuffering();
        }
    }
    else
    {
        if (m_bufferingEnabled)
        {
            m_bufferingEnabled = false;
            StopBuffering();
        }
    }
    return S_OK;
    */        
    return E_FAIL;
}

HRESULT MyHttpByteStream::StopBuffering()
{
    // NOTE: without this implementation, buffering event notification still works... why ?

    /*
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_bufferingInProgress)
    {
        std::cout << "MyHttpByteStream::StopBuffering start" << std::endl;
        m_bufferingInProgress = false;
        // Send MEBufferingStopped event
        SendEvent(MEBufferingStopped, GUID_NULL, S_OK, nullptr);
    }
    return S_OK;
    */
    return E_FAIL;
}

HRESULT MyHttpByteStream::SetBufferingParams(MFBYTESTREAM_BUFFERING_PARAMS *pParams)
{
    // ref: https://learn.microsoft.com/en-us/windows/win32/api/mfidl/ns-mfidl-mf_leaky_bucket_pair
    return S_OK;
}

// --------------------------------------------------------------------------
// IMFMediaEventGenerator implementation
// --------------------------------------------------------------------------

// NOTE: without 'IMFMediaEventGenerator' implementation, buffering event notification still works... why ?
#if 0
void MyHttpByteStream::StartBuffering() {
    if (m_bufferingInProgress)
        return;

    std::cout << "MyHttpByteStream::StartBuffering start" << std::endl;
    m_bufferingInProgress = true;
    // Send MEBufferingStarted event
    SendEvent(MEBufferingStarted, GUID_NULL, S_OK, nullptr);
}

void MyHttpByteStream::SendEvent(MediaEventType met, REFGUID guidType, HRESULT hrStatus, const PROPVARIANT* pv)
{
    if (!m_eventQueue) return;
    m_eventQueue->QueueEventParamVar(met, guidType, hrStatus, pv);
}

STDMETHODIMP MyHttpByteStream::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState)
{
    if (!m_eventQueue) return MF_E_NOT_INITIALIZED;
    return m_eventQueue->BeginGetEvent(pCallback, punkState);
}

STDMETHODIMP MyHttpByteStream::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    if (!m_eventQueue) return MF_E_NOT_INITIALIZED;
    return m_eventQueue->EndGetEvent(pResult, ppEvent);
}

STDMETHODIMP MyHttpByteStream::MyHttpByteStream::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    if (!m_eventQueue) return MF_E_NOT_INITIALIZED;
    return m_eventQueue->GetEvent(dwFlags, ppEvent);
}

STDMETHODIMP MyHttpByteStream::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    if (!m_eventQueue) return MF_E_NOT_INITIALIZED;
    return m_eventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
}
#endif