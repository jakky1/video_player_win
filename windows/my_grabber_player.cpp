#include "my_grabber_player.h"

#include <windows.h>

#include <ctime>
#include <iostream>
#include <mutex>

#pragma comment(lib, "D3D11")
#pragma comment(lib, "mfplat")

#define CHECK_HR(x) if (FAILED(x)) { goto done; }
#define NO_FRAME -2
#define TAG "[video_player_win][native] "

// --------------------------------------------------------------------------

STDMETHODIMP MyPlayer::QueryInterface(REFIID riid, void** ppv) 
{
    if (__uuidof(IMFMediaEngineNotify) == riid)
    {
        *ppv = static_cast<IMFMediaEngineNotify*>(this);
    }
    else
    {
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

HRESULT MyPlayer::initD3D11()
{
    static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
    };

    HRESULT hr;
    wil::com_ptr<IDXGIDevice2> pDXGIDevice;
    wil::com_ptr<ID3D10Multithread> pMultithread;

    hr = D3D11CreateDevice(
        m_adapter.get(),
        D3D_DRIVER_TYPE_UNKNOWN, //D3D_DRIVER_TYPE_HARDWARE, //TODO: should use HARDWARE... ??
        NULL,
        0,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &pDX11Device,
        NULL,
        NULL
    );
    CHECK_HR(hr);
   
    // enable multithread for d3d11 device
    CHECK_HR(hr = pDX11Device->QueryInterface(IID_PPV_ARGS(&pMultithread)));
    pMultithread->SetMultithreadProtected(TRUE);    

    UINT resetToken;
    CHECK_HR(hr = MFCreateDXGIDeviceManager(&resetToken, &m_pDXGIManager));
    CHECK_HR(hr = m_pDXGIManager->ResetDevice(pDX11Device.get(), resetToken));

    CHECK_HR(hr = pDX11Device->QueryInterface(IID_PPV_ARGS(&pDXGIDevice)));  
    
    // Ensure that DXGI does not queue more than one frame at a time. This both reduces 
    // latency and ensures that the application will only render after each VSync, minimizing 
    // power consumption.
    CHECK_HR(hr = pDXGIDevice->SetMaximumFrameLatency(1));

done:
    return hr;
}

HRESULT MyPlayer::initTexture()
{
    HRESULT hr;
    D3D11_TEXTURE2D_DESC textureDesc = {};

    textureDesc.Width = m_VideoWidth;
    textureDesc.Height = m_VideoHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;  
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    CHECK_HR(hr = pDX11Device->CreateTexture2D(&textureDesc, nullptr, &m_pTexture));

done:
    return hr;
}

MyPlayer::MyPlayer(IDXGIAdapter* adapter) :
    m_adapter(adapter),
    m_VideoWidth(0),
    m_VideoHeight(0),
    m_isShutdown(false)
{
    m_playingEvent = CreateEvent(NULL, TRUE, m_isPlaying, NULL);
}

HRESULT MyPlayer::EventNotify(DWORD event, DWORD_PTR param1, DWORD param2)
{
    switch (event) {
        case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE:
            return S_OK;

        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
            m_hasVideo = m_pEngine->HasVideo();
            if (m_hasVideo)
            {
                m_pEngine->GetNativeVideoSize(&m_VideoWidth, &m_VideoHeight);
                m_frameRectDst.right = m_VideoWidth;
                m_frameRectDst.bottom = m_VideoHeight;
                initTexture();
            }
            break;

        case MF_MEDIA_ENGINE_EVENT_CANPLAY:
            // notify client code that loading successfully
            m_loadCallback(true);
            m_loadCallback = NULL;
            break;

        case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY:
            updateFrame(); // show first frame when ready
            startVideoThread();
            break;

        // when playing / paused / ended, try to pause/resume video thread 
        case MF_MEDIA_ENGINE_EVENT_PLAY:
        case MF_MEDIA_ENGINE_EVENT_PLAYING:
            m_isPlaying = TRUE;
            SetEvent(m_playingEvent);
            break;
        case MF_MEDIA_ENGINE_EVENT_PAUSE:
        case MF_MEDIA_ENGINE_EVENT_ENDED:
            ResetEvent(m_playingEvent);
            m_isPlaying = FALSE;
            break;

        //case MF_MEDIA_ENGINE_EVENT_SEEKING:
        case MF_MEDIA_ENGINE_EVENT_SEEKED:
            if (!m_isPlaying && m_hasVideo) // video scrubbing in pause state
            {
                m_seekingToPts = -1;
                ResetEvent(m_playingEvent);
                updateFrame(); // TODO: seems not scrubbing during pause after seek...
            }
            break;

        case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED:
        case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED:
            break;

        case MF_MEDIA_ENGINE_EVENT_ERROR:
            // TODO: pass error reason to client code ?
            printErrorMessage(param1);
        case MF_MEDIA_ENGINE_EVENT_ABORT:
            if (m_loadCallback != NULL)
            {
                m_loadCallback(false);
                m_loadCallback = NULL;
            }
            break;
    }
    
    //std::cout << "EventNotify(): " << event << std::endl;
    OnPlayerEvent(event);
    return S_OK;
}

void MyPlayer::printErrorMessage(DWORD_PTR param1)
{
    char *msg = "Unknown error";
    switch (param1)
    {
        case MF_MEDIA_ENGINE_ERR_NOERROR:
            msg = "no error... ??";
            break;
        case MF_MEDIA_ENGINE_ERR_ABORTED :
            msg = "aborted";
            break;
        case MF_MEDIA_ENGINE_ERR_NETWORK :
            msg = "network issue";
            break;
        case MF_MEDIA_ENGINE_ERR_DECODE :
            msg = "decode error";
            break;
        case MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED :
            msg = "file not found / corrupted / not supported";
            break;
        case MF_MEDIA_ENGINE_ERR_ENCRYPTED:
            msg = "file is encrypted";
            break;
    }
    std::cout << TAG "player error occurs (MF_MEDIA_ENGINE_EVENT_ERROR) : " << msg << std::endl;
}

HRESULT MyPlayer::OpenURL(const WCHAR* pszFileName, MyPlayerCallback* playerCallback, HWND hwndVideo, std::function<void(bool)> loadCallback)
{
    HRESULT hr;
    wil::com_ptr<IMFMediaEngineClassFactory> pFactory; // TODO: keep as static member ?
    wil::com_ptr<IMFAttributes> pAttributes;    

    if (m_isShutdown || m_pEngine) return E_ABORT;

    m_frameCallback = playerCallback;
    m_loadCallback = loadCallback;

    CHECK_HR(hr = initD3D11());
    CHECK_HR(hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory)));

    CHECK_HR(hr = MFCreateAttributes(&pAttributes, 3));
    CHECK_HR(hr = pAttributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, (IUnknown*)m_pDXGIManager.get()));
    CHECK_HR(hr = pAttributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, (IUnknown*)this));
    CHECK_HR(hr = pAttributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM));
    CHECK_HR(hr = pFactory->CreateInstance(0, pAttributes.get(), &m_pEngine));
    
    CHECK_HR(hr = m_pEngine->QueryInterface(IID_PPV_ARGS(&m_pEngineEx)));

	CHECK_HR(hr = m_pEngine->SetSource((BSTR)pszFileName));

done:
    if (FAILED(hr)) 
    {
        std::cout << TAG "OpenURL() failed: hr=" << hr << std::endl;
        m_loadCallback(SUCCEEDED(hr));
        m_loadCallback = NULL;
    }
    return hr;
}

MyPlayer::~MyPlayer()
{
    Shutdown();
    CloseHandle(m_playingEvent);
    std::cout << TAG "~MyPlayer() destroyed" << std::endl;
}

DWORD WINAPI MyThreadFunction(LPVOID lpParam)
{
    wil::com_ptr<MyPlayer> player((MyPlayer*) lpParam); // keep player reference during thread
    player->priv__videoThreadFunc();
    return 0;
}

HRESULT MyPlayer::startVideoThread()
{
    if (!m_hasVideo) return S_OK; // no video track found
    if (NULL != m_threadHandle) {
        std::cout << "startVideoThread() already running now !!!" << std::endl;
        return E_FAIL;
    }

    // TODO: should call this->AddRef() before thread start ???
    m_threadHandle = CreateThread(NULL, 0, MyThreadFunction, this, 0, NULL);
    if (NULL == m_threadHandle) {
        return E_FAIL;
    }
    SetThreadPriority(m_threadHandle, THREAD_PRIORITY_HIGHEST);

    return S_OK;
}

void MyPlayer::priv__videoThreadFunc()
{
    wil::com_ptr<IDXGIOutput> pDXGIOutput;

    m_adapter->EnumOutputs(0, &pDXGIOutput);

    //std::cout << "priv__videoThreadFunc() start" << std::endl;
    do 
    {
        pDXGIOutput->WaitForVBlank();

        // show frame
        if (m_isShutdown) break;
        updateFrame();

        // pause thread if video paused
        if (m_isShutdown) break;
        if (!m_isPlaying && m_seekingToPts < 0)
        {
            // suspend thread during video paused, wait until play() or shutdown()
            //std::cout << "video thread pause ~~~" << std::endl;
            WaitForSingleObject(m_playingEvent, INFINITE);
            //std::cout << "video thread resume ~~~" << std::endl;
        }

    } while (true);

    //std::cout << "priv__videoThreadFunc() exit" << std::endl;
    CloseHandle(m_threadHandle);
    m_threadHandle = NULL;
}

LONGLONG MyPlayer::updateFrame()
{
    HRESULT hr;
    LONGLONG pts;
    bool bFound;

    do
    {
        pts = -1;
        bFound = false;

        hr = m_pEngine->OnVideoStreamTick(&pts);
        //std::cout << "OnVideoStreamTick() hr: " << hr << ", pts : " << pts << std::endl;
        if (S_OK == hr)
        {
            /*
            static LONGLONG s_lastPts = 0;
            std::cout << "frame diff pts : " << (pts - s_lastPts) / 10000 << std::endl;
            s_lastPts = pts;
            */

            hr = m_pEngine->TransferVideoFrame(m_pTexture.get(), &m_frameRectSrc, &m_frameRectDst, NULL);
            if (FAILED(hr)) {
                std::cout << TAG "TransferVideoFrame failed !!!!!!!!!!!! hr = " << hr << std::endl;
            }
            else 
            {
                m_frameCallback->OnProcessFrame(m_pTexture.get());
                bFound = true;
            }
        }
        else 
        {
            hr = S_OK;
            //std::cout << "OnVideoStreamTick() no new frame !!!!!" << std::endl;
        }

        // video scrubbing: if seeking in pause state, loop until next frame found
        if (!m_isPlaying && m_seekingToPts >= 0)
        {
            LONGLONG diffPts = pts - m_seekingToPts;
            if (diffPts < 0) diffPts = -diffPts;
            if (bFound && diffPts < 10000*100)
            {
                m_seekingToPts = -1;                
                ResetEvent(m_playingEvent); // pause video thread again
                return -1;
            }
            else
            {
                continue;
            }
        }
    } while (false);

    return bFound ? pts : NO_FRAME;
}

HRESULT MyPlayer::Play(LONGLONG ms)
{
    if (!m_pEngine) return E_FAIL;
   	return m_pEngine->Play();
}

HRESULT MyPlayer::Pause()
{
    if (!m_pEngine) return E_FAIL;
    return m_pEngine->Pause();
}

LONGLONG MyPlayer::GetDuration()
{
    if (!m_pEngine) return -1;
    return (LONGLONG) (m_pEngine->GetDuration() * 1000);
}

LONGLONG MyPlayer::GetCurrentPosition()
{
    if (!m_pEngine) return -1;
    return (LONGLONG) (m_pEngine->GetCurrentTime() * 1000);
}

HRESULT MyPlayer::Seek(LONGLONG ms)
{
    if (!m_pEngine) return E_FAIL;

    if (!m_isPlaying) 
    {
        // video scrubbing
        m_seekingToPts = ms * 10000;
        SetEvent(m_playingEvent);
    }
    //return m_pEngine->SetCurrentTime((double)ms / 1000);
    return m_pEngineEx->SetCurrentTimeEx((double)ms / 1000, MF_MEDIA_ENGINE_SEEK_MODE_APPROXIMATE );
}

SIZE MyPlayer::GetVideoSize()
{
    SIZE size = {};
    if (!m_pEngine) return size;
    size.cx = (LONG) m_VideoWidth;
    size.cy = (LONG) m_VideoHeight;
    return size;
}

HRESULT MyPlayer::SetPlaybackSpeed(float speed)
{
    if (!m_pEngine) return E_FAIL;
    return m_pEngine->SetPlaybackRate((double)speed);
}

HRESULT MyPlayer::GetVolume(float* pVol)
{
    if (!m_pEngine) return E_FAIL;
    double vol = m_pEngine->GetVolume();
    *pVol = (float)vol;
    return S_OK;
}

HRESULT MyPlayer::SetVolume(float vol)
{
    if (!m_pEngine) return E_FAIL;
    return m_pEngine->SetVolume((double)vol);
}

void MyPlayer::Shutdown()
{
    std::unique_lock<std::mutex> guard(m_mutex);
    if (m_isShutdown) return;
    m_isShutdown = true;

    if (m_pEngine) 
    {
        OnPlayerEvent(MESessionClosed);
        SetEvent(m_playingEvent); // resume video thread and close by itself
        m_pEngine->Shutdown();
        m_pEngine.reset();
        m_pTexture.reset();
        
        this->Release(); // TODO: without this line, ~MyPlayer() not called... who keep this pointer ???
    }
}
