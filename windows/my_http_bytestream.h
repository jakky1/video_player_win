#pragma once

#include <mfobjects.h> // IMFByteStream
#include <mfidl.h> // IMFByteStreamBuffering
#include <wil/com.h>
#include <vector>
#include <string>
#include <mutex>

class MyHttpConnection;
class MyHttpByteStream : 
    public IMFByteStream,
    public IMFByteStreamBuffering
    //public IMFMediaEventGenerator
{
public:
    MyHttpByteStream(const std::wstring& url, const std::vector<std::wstring> headers = {});
    ~MyHttpByteStream();

    // IMFByteStreamBuffering
    std::mutex m_mutex;
    bool m_bufferingEnabled = false;
    bool m_bufferingInProgress = false;
    HRESULT EnableBuffering(BOOL fEnable);
    HRESULT SetBufferingParams(MFBYTESTREAM_BUFFERING_PARAMS *pParams);
    HRESULT StopBuffering();
    //

    // IMFMediaEventGenerator
#if 0
    wil::com_ptr<IMFMediaEventQueue> m_eventQueue;
    void SendEvent(MediaEventType met, REFGUID guidType, HRESULT hrStatus, const PROPVARIANT* pv);
    void StartBuffering();
    //void StopBuffering();
    HRESULT BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState);
    HRESULT EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
    HRESULT GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent);
    HRESULT QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue);
#endif
    //

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

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
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
