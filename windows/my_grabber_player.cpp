// ref: https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/medfound/using-the-sample-grabber-sink.md
// ref: https://learn.microsoft.com/en-us/windows/win32/medfound/seeking--fast-forward--and-reverse-play
// ref: https://blog.csdn.net/u013113678/article/details/125492286
// ref: https://blog.csdn.net/weixin_40256196/article/details/127021206

#include "my_grabber_player.h"

#include <Shlwapi.h>
//#include <mfapi.h>
//#include <mfidl.h>
#include <mfreadwrite.h>
#include <new>
#include <iostream>

#include <mmdeviceapi.h>
#include <audiopolicy.h>

#pragma comment(lib, "mf")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "Shlwapi")
#pragma comment(lib, "Mfreadwrite")

#define CHECK_HR(x) if (FAILED(x)) { goto done; }

class CAsyncCallback : public IMFAsyncCallback
{
    // ref: https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/DX11VideoRenderer/cpp/Common.h
public:

    typedef std::function<HRESULT(IMFAsyncResult*)> InvokeFn;
    //typedef HRESULT(InvokeFn)(IMFAsyncResult* pAsyncResult);

    CAsyncCallback(InvokeFn fn) :
        m_pInvokeFn(fn)
    {
    }

    // IUnknown
    inline STDMETHODIMP_(ULONG) AddRef() {
        return InterlockedIncrement(&m_cRef);
    }
    STDMETHODIMP_(ULONG) Release() {
        ULONG uCount = InterlockedDecrement(&m_cRef);
        if (uCount == 0) delete this;
        return uCount;
    }

    STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
    {
        if (!ppv)
        {
            return E_POINTER;
        }
        if (iid == __uuidof(IUnknown))
        {
            *ppv = static_cast<IUnknown*>(static_cast<IMFAsyncCallback*>(this));
        }
        else if (iid == __uuidof(IMFAsyncCallback))
        {
            *ppv = static_cast<IMFAsyncCallback*>(this);
        }
        else
        {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue)
    {
        // Implementation of this method is optional.
        return E_NOTIMPL;
    }

    STDMETHODIMP Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult)
    {
        return (m_pInvokeFn)(pAsyncResult);
    }

private:

    long m_cRef = 1;
    InvokeFn m_pInvokeFn;
};

HRESULT CreateTopology(IMFMediaSource* pSource, IMFActivate* pSink, IMFTopology** ppTopo);

// --------------------------------------------------------------------------

MyPlayer::MyPlayer() :
    m_hnsDuration(-1),
    m_VideoWidth(0),
    m_VideoHeight(0),
    m_vol(1.0),
    m_isUserAskPlaying(false),
    m_isShutdown(false)
{
    // do nothing
}

MyPlayer::~MyPlayer()
{
    Shutdown();
    CloseWindow(m_ChildWnd);
    std::cout << "[native] ~MyPlayer()" << std::endl;
}

HRESULT MyPlayer::OpenURL(const WCHAR* pszFileName, MyPlayerCallback* playerCallback, HWND hwndVideo, std::function<void(bool)> loadCallback)
{
    // save parameters in OpenURL(), used to re-open when open failed
    std::wstring filename = std::wstring(pszFileName);
    m_reopenFunc = [=](std::function<void(bool)> cb) {
        OpenURL(filename.c_str(), playerCallback, hwndVideo, cb);
    };
    //

    HRESULT hr = CreateMediaSourceAsync(pszFileName, [=](IMFMediaSource* pSource) -> void {
        HRESULT hr;
        wil::com_ptr<IMFTopology> pTopology;
        wil::com_ptr<IMFClock> pClock;

        if (pSource == NULL) {
            //load fail or abort
            loadCallback(false);
            m_reopenFunc = NULL;
            return; // *this* maybe already deleted, so don't access any *this members, and return immediately!
        }
        m_pMediaSource = pSource;
        pSource = NULL;

        {
            //CHECK_HR(hr = MFCreateVideoRendererActivate(hwndVideo, &m_pVideoSinkActivate)); //Jacky
            D3D11Texture2DCallback frameCB = NULL;
            if (playerCallback != NULL) {
                frameCB = [playerCallback](ID3D11Texture2D* texture) -> void {
                    playerCallback->OnProcessFrame(texture);
                };
            }
            CHECK_HR(hr = CreateDX11VideoRendererActivate(hwndVideo, &m_pVideoSinkActivate, frameCB)); //Jacky
        }

        // Create the Media Session.
        CHECK_HR(hr = MFCreateMediaSession(NULL, &m_pSession));

        // Create the topology.
        CHECK_HR(hr = CreateTopology(m_pMediaSource.get(), m_pVideoSinkActivate.get(), &pTopology));

        // Run the media session.
        CHECK_HR(hr = m_pSession->SetTopology(0, pTopology.get()));

        // Get the presentation clock (optional)
        CHECK_HR(hr = m_pSession->GetClock(&pClock));
        CHECK_HR(hr = pClock->QueryInterface(IID_PPV_ARGS(&m_pClock)));

        // Get the rate control interface (optional)
        CHECK_HR(MFGetService(m_pSession.get(), MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS(&m_pRate)));

        // add event listener
        m_pSession->BeginGetEvent(this, NULL);

        if (m_isShutdown) hr = E_FAIL;

    done:
        // Clean up.
        if (FAILED(hr)) Shutdown();
        loadCallback(SUCCEEDED(hr));
    });

    // Clean up.
    if (FAILED(hr)) Shutdown();
    return hr;
}

HRESULT MyPlayer::Play(LONGLONG ms)
{
    if (m_pSession == NULL) return E_FAIL;
    if (ms >= 0) return Seek(ms);

    m_isUserAskPlaying = true;
    doSetVolume(m_vol);

    PROPVARIANT var;
    PropVariantInit(&var);
    m_pSession->Pause(); //workaround: prevent video freeze when call Play() twice
    return m_pSession->Start(NULL, &var);
}

HRESULT MyPlayer::Pause()
{
    if (m_pSession == NULL) return E_FAIL;
    m_isUserAskPlaying = false;
    return m_pSession->Pause();
}

LONGLONG MyPlayer::GetDuration()
{
    if (m_pSession == NULL) return -1;
    return m_hnsDuration / 10000;
}

LONGLONG MyPlayer::GetCurrentPosition()
{
    MFTIME pos;
    HRESULT hr;
    if (m_pSession == NULL) return -1;
    hr = m_pClock->GetTime(&pos);
    if (FAILED(hr)) return -1;
    return pos / 10000;
}

HRESULT MyPlayer::Seek(LONGLONG ms)
{
    PROPVARIANT var;
    if (m_pSession == NULL) return E_FAIL;

    PropVariantInit(&var);
    var.vt = VT_I8;
    var.hVal.QuadPart = ms * 10000;

    // if seek in pause state, mute the volume
    if (!m_isUserAskPlaying) doSetVolume(0.0f);
    HRESULT hr = m_pSession->Start(NULL, &var);
    if (!m_isUserAskPlaying) m_pSession->Pause();

    return hr;
}

SIZE MyPlayer::GetVideoSize()
{
    SIZE size = {};
    size.cx = m_VideoWidth;
    size.cy = m_VideoHeight;
    return size;
}

HRESULT MyPlayer::SetPlaybackSpeed(float speed)
{
    if (m_pSession == NULL) return E_FAIL;
    return m_pRate->SetRate(FALSE, speed);
}

HRESULT MyPlayer::initAudioVolume()
{
    if (m_pAudioVolume) return S_OK;
    if (!m_pAudioRendererActivate) return E_FAIL;

    HRESULT hr;
    wil::com_ptr<IMFGetService> pGetService;
    CHECK_HR(hr = m_pAudioRendererActivate->ActivateObject(IID_PPV_ARGS(&pGetService)));
    CHECK_HR(hr = pGetService->GetService(MR_STREAM_VOLUME_SERVICE, IID_PPV_ARGS(&m_pAudioVolume)));
done:
    return hr;
}

HRESULT MyPlayer::GetVolume(float* pVol)
{
    *pVol = m_vol;
    return S_OK;
}

HRESULT MyPlayer::SetVolume(float vol)
{
    HRESULT hr = doSetVolume(vol);
    if (SUCCEEDED(hr)) {
        m_vol = vol;
    }
    return hr;
}

HRESULT MyPlayer::doSetVolume(float fVol)
{
    HRESULT hr;
    UINT32 channelsCount;
    float volumes[30];

    CHECK_HR(hr = initAudioVolume());
    CHECK_HR(hr = m_pAudioVolume->GetChannelCount(&channelsCount));
    for (UINT32 i = 0; i < channelsCount; i++) volumes[i] = fVol;
    CHECK_HR(hr = m_pAudioVolume->SetAllVolumes(channelsCount, volumes));

done:
    return hr;
}

void MyPlayer::Shutdown()
{
    std::unique_lock<std::mutex> guard(m_mutex);
    if (m_isShutdown) return;
    m_isShutdown = true;
    m_hnsDuration = -1;
    cancelAsyncLoad();

    // NOTE: because m_pSession->BeginGetEvent(this) will keep *this,
    //       so we need to call m_pSession->Shutdown() first
    //       then client call player->Release() will make refCount = 0
    if (m_pSession) {
        m_pSession->Shutdown();
        m_pMediaSource->Shutdown(); // will memory-leak if not shutdown
    }
}

void MyPlayer::cancelAsyncLoad() {
    if (m_pSourceResolver != NULL && m_pSourceResolverCancelCookie != NULL) {
        m_pSourceResolver->CancelObjectCreation(m_pSourceResolverCancelCookie.get());
        m_pSourceResolver.reset();
        m_pSourceResolverCancelCookie.reset();
    }
}

HRESULT MyPlayer::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
    return S_OK;
}

HRESULT MyPlayer::Invoke(IMFAsyncResult* pResult)
{
    //std::unique_lock<std::mutex> guard(m_mutex);
    wil::com_ptr<MyPlayer> thisRef(this);
    HRESULT hr;
    wil::com_ptr<IMFMediaEvent> pEvent;
    MediaEventType meType = MEUnknown;

    if (m_isShutdown || m_pSession == NULL) return E_FAIL;
    CHECK_HR(hr = m_pSession->EndGetEvent(pResult, &pEvent));
    CHECK_HR(hr = pEvent->GetType(&meType));

    //std::cout << "native player event: " << meType << std::endl;

    if (!m_topoSet) {
        if (meType == MESessionNotifyPresentationTime) {
            if (!m_isUserAskPlaying) {
                // workaround: show the first frame when video loaded and Play() not called
                Play();
                Pause();
            } else {
                Play();
            }
            m_topoSet = true;
            m_reopenFunc = NULL;
        } else if (meType == MESessionPaused) {
            // workaround: something wrong with topology, re-open now
            std::cout << "[video_player_win] load fail, reload now" << std::endl;
            Shutdown();
            m_isShutdown = false;
            m_reopenFunc([=](bool bSuccess) {
                if (!bSuccess) {
                    OnPlayerEvent(MEError);
                }
            });
            return S_OK;
        }
    }

    CHECK_HR(hr = m_pSession->BeginGetEvent(this, NULL));
    switch (meType) {
    case MESessionStarted:
    case MEBufferingStarted:
    case MEBufferingStopped:
    case MESessionPaused:
    case MESessionStopped:
    case MESessionClosed:
    case MESessionEnded:
    case MEError:
        OnPlayerEvent(meType);
        break;
    }

done:
    return S_OK;
}

// --------------------------------------------------------------------------

// Create a media source from a URL.
HRESULT MyPlayer::CreateMediaSourceAsync(PCWSTR pszURL, std::function<void(IMFMediaSource* pSource)> callback)
{
    // Create the source resolver.
    HRESULT hr = S_OK;
    CAsyncCallback* cb = NULL;
    CHECK_HR(hr = MFCreateSourceResolver(&m_pSourceResolver));

    hr = m_pSourceResolver->BeginCreateObjectFromURL(pszURL,
        MF_RESOLUTION_MEDIASOURCE, NULL, &m_pSourceResolverCancelCookie,
        cb = new CAsyncCallback([=](IMFAsyncResult* pResult) -> HRESULT {
            HRESULT hr;
            MF_OBJECT_TYPE ObjectType;
            wil::com_ptr<IUnknown> pSource;
            wil::com_ptr<IMFMediaSource> pMediaSource;

            if (m_isShutdown) {
                //pResult->Release();
                callback(NULL);
                return E_FAIL; // *this* maybe already deleted, so don't access any *this members, and return immediately!
            }

            if (m_pSourceResolver) { // m_pSourceResolver maybe null since Shutdown() called immediately after OpenURL()
                CHECK_HR(hr = m_pSourceResolver->EndCreateObjectFromURL(pResult, &ObjectType, &pSource));
                CHECK_HR(hr = pSource->QueryInterface(IID_PPV_ARGS(&pMediaSource)));
            }
            else
            {
                hr = E_FAIL;
                goto done;
            }

            pMediaSource->AddRef();
            callback(pMediaSource.get());

        done:
            //m_pSourceResolver.reset();
            //m_pSourceResolverCancelCookie.reset();
            if (FAILED(hr)) callback(NULL);
            return S_OK;
            }),
        NULL);
    CHECK_HR(hr);

done:
    if (cb) cb->Release();
    if (FAILED(hr)) {
        m_pSourceResolver.reset();
        m_pSourceResolverCancelCookie.reset();
    }
    return hr;
}


// Add a source node to a topology.
HRESULT AddSourceNode(
    IMFTopology* pTopology,           // Topology.
    IMFMediaSource* pSource,          // Media source.
    IMFPresentationDescriptor* pPD,   // Presentation descriptor.
    IMFStreamDescriptor* pSD,         // Stream descriptor.
    IMFTopologyNode** ppNode)         // Receives the node pointer.
{
    wil::com_ptr<IMFTopologyNode> pNode;

    HRESULT hr = S_OK;
    CHECK_HR(hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode));
    CHECK_HR(hr = pNode->SetUnknown(MF_TOPONODE_SOURCE, pSource));
    CHECK_HR(hr = pNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD));
    CHECK_HR(hr = pNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD));
    CHECK_HR(hr = pTopology->AddNode(pNode.get()));

    // Return the pointer to the caller.
    *ppNode = pNode.get();
    (*ppNode)->AddRef();

done:
    return hr;
}

// Add an output node to a topology.
HRESULT AddOutputNode(
    IMFTopology* pTopology,     // Topology.
    IMFActivate* pActivate,     // Media sink activation object.
    DWORD dwId,                 // Identifier of the stream sink.
    IMFTopologyNode** ppNode)   // Receives the node pointer.
{
    wil::com_ptr<IMFTopologyNode> pNode;

    HRESULT hr = S_OK;
    CHECK_HR(hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pNode));
    CHECK_HR(hr = pNode->SetObject(pActivate));
    //CHECK_HR(hr = pNode->SetUINT32(MF_TOPONODE_STREAMID, dwId));
    //CHECK_HR(hr = pNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE));
    CHECK_HR(hr = pTopology->AddNode(pNode.get()));

    // Return the pointer to the caller.
    *ppNode = pNode.get();
    (*ppNode)->AddRef();

done:
    return hr;
}

// Create the topology.
HRESULT MyPlayer::CreateTopology(IMFMediaSource* pSource, IMFActivate* pSinkActivate, IMFTopology** ppTopo)
{
    wil::com_ptr<IMFTopology> pTopology;
    wil::com_ptr<IMFPresentationDescriptor> pPD;
    wil::com_ptr<IMFStreamDescriptor> pSD;
    wil::com_ptr<IMFMediaTypeHandler> pHandler;
    wil::com_ptr<IMFTopologyNode> pNodeSrc; // source node
    wil::com_ptr<IMFTopologyNode> pNodeVideoSink; // video node
    wil::com_ptr<IMFTopologyNode> pNodeAudioSink; // audio node
    wil::com_ptr<IMFMediaType> pVideoMediaType;
    bool isSourceAdded = false;

    HRESULT hr = S_OK;
    DWORD cStreams = 0;

    m_VideoWidth = m_VideoHeight = 0;

    CHECK_HR(hr = MFCreateTopology(&pTopology));
    CHECK_HR(hr = pSource->CreatePresentationDescriptor(&pPD));
    CHECK_HR(hr = pPD->GetStreamDescriptorCount(&cStreams));

    for (DWORD i = 0; i < cStreams; i++)
    {
        // In this example, we look for audio streams and connect them to the sink.

        BOOL fSelected = FALSE;
        GUID majorType;

        CHECK_HR(hr = pPD->GetStreamDescriptorByIndex(i, &fSelected, &pSD));
        CHECK_HR(hr = pSD->GetMediaTypeHandler(&pHandler));
        CHECK_HR(hr = pHandler->GetMajorType(&majorType));

        if (majorType == MFMediaType_Video && fSelected) //Jacky
        {
            if (!isSourceAdded)
            {
                CHECK_HR(hr = AddSourceNode(pTopology.get(), pSource, pPD.get(), pSD.get(), &pNodeSrc));
                isSourceAdded = true;
            }

            CHECK_HR(hr = AddSourceNode(pTopology.get(), pSource, pPD.get(), pSD.get(), &pNodeSrc));
            CHECK_HR(hr = AddOutputNode(pTopology.get(), pSinkActivate, 0, &pNodeVideoSink));
            CHECK_HR(hr = pNodeSrc->ConnectOutput(0, pNodeVideoSink.get(), 0));

            // get video resolution
            CHECK_HR(hr = pHandler->GetCurrentMediaType(&pVideoMediaType));
            MFGetAttributeSize(pVideoMediaType.get(), MF_MT_FRAME_SIZE, &m_VideoWidth, &m_VideoHeight);
        }
        else if (majorType == MFMediaType_Audio && fSelected)
        {
            //Jacky
            if (!isSourceAdded)
            {
                CHECK_HR(hr = AddSourceNode(pTopology.get(), pSource, pPD.get(), pSD.get(), &pNodeSrc));
                isSourceAdded = true;
            }
            CHECK_HR(hr = MFCreateAudioRendererActivate(&m_pAudioRendererActivate));
            CHECK_HR(hr = AddOutputNode(pTopology.get(), m_pAudioRendererActivate.get(), 0, &pNodeAudioSink));
            CHECK_HR(hr = pNodeSrc->ConnectOutput(0, pNodeAudioSink.get(), 0));
        }
        else
        {
            CHECK_HR(hr = pPD->DeselectStream(i));
        }
    }

    CHECK_HR(pPD->GetUINT64(MF_PD_DURATION, (UINT64*)&m_hnsDuration));

    *ppTopo = pTopology.get();
    (*ppTopo)->AddRef();

done:
    return hr;
}
