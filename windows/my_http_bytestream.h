#pragma once

#include <mfobjects.h> // IMFByteStream
#include <wil/com.h>
#include <vector>
#include <string>

class MyHttpConnection;
class MyHttpByteStream : public IMFByteStream
{
public:
    MyHttpByteStream(const std::wstring& url, const std::vector<std::wstring> headers = {});
    ~MyHttpByteStream();

    HRESULT GetCapabilities(DWORD* pdwCapabilities);

    HRESULT Read(BYTE* pb, ULONG cb, ULONG* pcbRead);
    HRESULT BeginRead(BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState);
    HRESULT EndRead(IMFAsyncResult* pResult, ULONG* pcbRead);

    HRESULT Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin,
        LONGLONG llSeekOffset,
        DWORD dwSeekFlags,
        QWORD* pqwCurrentPosition
    );
    HRESULT SetCurrentPosition(QWORD qwPosition);
    HRESULT GetCurrentPosition(QWORD* pqwPosition);
    HRESULT GetLength(QWORD* pqwLength);
    HRESULT IsEndOfStream(BOOL* pfEndOfStream);
    HRESULT Close();

    // invalid operations
    HRESULT Flush() { return E_FAIL; }
    HRESULT SetLength(QWORD qwLength) { return E_FAIL; }
    HRESULT Write(const BYTE* pb, ULONG cb, ULONG* pcbWritten) { return E_FAIL; }
    HRESULT BeginWrite(const BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState) { return E_FAIL; }
    HRESULT EndWrite(IMFAsyncResult* pResult, ULONG* pcbWritten) { return E_FAIL; }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (riid == IID_IUnknown || riid == IID_IMFByteStream) {
            *ppv = static_cast<IMFByteStream*>(this);
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
    bool openConnection(QWORD position);

    MyHttpConnection* mConnection = NULL;
    std::wstring mUrl;
    std::vector<std::wstring> mHeaders;
    QWORD mPosition = 0;
    QWORD mFileSize = 0;
};
