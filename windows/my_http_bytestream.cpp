#include "my_http_bytestream.h"

#include <windows.h>
#include <winhttp.h>
#include <mfobjects.h> // IMFByteStream
#include <mfapi.h> // MFCreateAsyncResult

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

    // 解析 URL
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

    // 建立连接
    hConnect = WinHttpConnect(hSession, urlComp.lpszHostName,
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    CHECK(hConnect, "WinHttpConnect failed");

    // 创建请求
    hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath,
        NULL, WINHTTP_NO_REFERER,
        NULL, WINHTTP_FLAG_SECURE);
    CHECK(hRequest, "WinHttpOpenRequest failed");

    // 新增 Range 頭以從指定位置開始下載
    if (startPosition > 0)
    {
        wchar_t rangeHeader[50];
        swprintf(rangeHeader, sizeof(rangeHeader) / sizeof(wchar_t), L"Range: bytes=%d-", (int)startPosition);
        WinHttpAddRequestHeaders(hRequest, rangeHeader, (DWORD) wcslen(rangeHeader), (DWORD) WINHTTP_ADDREQ_FLAG_ADD);
    }

    std::wcout << L"start pass headers..." << std::endl;
    for (auto header : headers)
    {
        std::wcout << L"header: " << header << std::endl;
        BOOL b = WinHttpAddRequestHeaders(hRequest, header.c_str(), (DWORD) header.length(), (DWORD) WINHTTP_ADDREQ_FLAG_ADD);
        CHECK(b, "WinHttpAddRequestHeaders failed");
    }

    // 发送请求
    bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA,
        0, 0, 0);
    CHECK(bResults, "WinHttpSendRequest failed");

    // 接收响应
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
        std::wcout << L"Content Length: " << contentLength << L" bytes" << std::endl;
        return contentLength;
    }

    return -1;
}

MyHttpConnection::~MyHttpConnection()
{
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
    //std::cout << "MyHttpByteStream::GetLength" << std::endl;
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
    //std::cout << "MyHttpByteStream::IsEndOfStream" << std::endl;
    if (!SUCCEEDED(GetLength(&size))) return E_FAIL;
    *pfEndOfStream = (mPosition >= size);
    //std::cout << "MyHttpByteStream::IsEndOfStream end : " << (bool)(*pfEndOfStream) << std::endl;
    return S_OK;
}

bool MyHttpByteStream::openConnection(QWORD position)
{
    //std::cout << "MyHttpByteStream::openConnection" << std::endl;
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
    //if (!mConnection) return E_FAIL;
    *pqwPosition = mPosition;
    //std::cout << "MyHttpByteStream::GetCurrentPosition : mPosition = " << (*pqwPosition) << std::endl;
    return S_OK;
}

HRESULT MyHttpByteStream::SetCurrentPosition(QWORD qwPosition)
{
    //std::cout << "MyHttpByteStream::SetCurrentPosition : pos = " << qwPosition << std::endl;
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
    //std::cout << "MyHttpByteStream::Seek : SeekOrigin = " << SeekOrigin << " , llSeekOffset = " << llSeekOffset << std::endl;
    if (msoBegin == SeekOrigin) pos = (long) llSeekOffset;
    else pos = (long) mPosition + (long) llSeekOffset;
    return SetCurrentPosition(pos);
}

HRESULT MyHttpByteStream::Read(BYTE* pb, ULONG cb, ULONG* pcbRead)
{
    //std::cout << "MyHttpByteStream::Read start : ask len = " << cb << std::endl;
    if (!mConnection)
    {
        if (!openConnection(mPosition)) return E_FAIL;
    }

    int totalLen = 0;
    while (cb > 0)
    {
        int len = mConnection->read(pb, cb);
        //std::cout << "read len = " << len << std::endl;
        if (len < 0) break;
        mPosition += len;
        pb += len;
        totalLen += len;
        cb -= len;
        break;
    }
    *pcbRead = totalLen;
    //std::cout << "MyHttpByteStream::Read end : len = " << (*pcbRead) << std::endl;
    return S_OK; 
}

HRESULT MyHttpByteStream::BeginRead(BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState)
{
    //std::cout << "MyHttpByteStream::BeginRead" << std::endl;
    if (!pb || cb == 0 || !pCallback) {
        return E_POINTER;
    }

    auto ret = std::async(std::launch::async, [=]() -> void {
        wil::com_ptr<IMFAsyncResult> pAsyncResult;
        wil::com_ptr<MyDataIUnknown> pData;

        ULONG readLen = 0;
        Read(pb, cb, &readLen);

        pData = new MyDataIUnknown(readLen);
        MFCreateAsyncResult(pData.get(), pCallback, punkState, &pAsyncResult);
        //std::cout << "MyHttpByteStream::BeginRead before Invoke()" << std::endl;
        MFInvokeCallback(pAsyncResult.get());
        //std::cout << "MyHttpByteStream::BeginRead after invoke()" << std::endl;
        });
    return S_OK;
}

HRESULT MyHttpByteStream::EndRead(IMFAsyncResult* pResult, ULONG* pcbRead)
{
    //std::cout << "MyHttpByteStream::EndRead start" << std::endl;
    if (!pResult || !pcbRead) {
        return E_POINTER;
    }

    wil::com_ptr<MyDataIUnknown> pData;
    pResult->GetObject((IUnknown**)&pData);
    if (!pData) return E_FAIL;
    *pcbRead = pData->getData();
    //std::cout << "MyHttpByteStream::EndRead end , *pcbRead = " << (*pcbRead) << std::endl;
    return S_OK;
}

HRESULT MyHttpByteStream::Close()
{
    //std::cout << "MyHttpByteStream::Close" << std::endl;
    if (mConnection)
    {
        delete mConnection;
        mConnection = NULL;
        return S_OK;
    }
    return E_FAIL;
}

MyHttpByteStream::MyHttpByteStream(const std::wstring& url, const std::vector<std::wstring> headers)
{
    std::cout << "MyHttpByteStream::MyFileByteStream()" << std::endl;
    mUrl = url;
    mHeaders = headers;
}

MyHttpByteStream::~MyHttpByteStream()
{
    std::cout << "MyHttpByteStream::~MyFileByteStream()" << std::endl;
    Close();
}
