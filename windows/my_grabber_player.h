#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <dxgi1_2.h>
#include <d3d11.h>

#include <iostream>
#include <functional>
#include <mutex>

#include <wil/com.h>

class MyPlayerCallback : public IUnknown
{
public:
	virtual void OnProcessFrame(ID3D11Texture2D* texture) = 0;
};

class MyPlayer : public IMFMediaEngineNotify
{
public:
	STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
	inline STDMETHODIMP_(ULONG) AddRef() {
		return InterlockedIncrement(&m_cRef);
	}
	STDMETHODIMP_(ULONG) Release() {
		ULONG uCount = InterlockedDecrement(&m_cRef);
		if (uCount == 0) delete this;
		return uCount;
	}
private:
	long m_cRef = 1;

public:
	HRESULT OpenURL(const WCHAR* pszFileName, MyPlayerCallback* playerCallback, HWND hwndVideo, std::function<void(bool)> loadCallback);
	HRESULT Play(LONGLONG ms = -1);
	HRESULT Pause();
	void Shutdown();

	LONGLONG GetDuration();
	LONGLONG GetCurrentPosition();
	HRESULT Seek(LONGLONG ms);
	SIZE GetVideoSize();

	HRESULT SetPlaybackSpeed(float fRate);

	HRESULT GetVolume(float* pVol);
	HRESULT SetVolume(float vol);

	MyPlayer(IDXGIAdapter* adapter);
	virtual ~MyPlayer();

	void priv__videoThreadFunc();

protected:
	virtual void OnPlayerEvent(DWORD event) {};

	DWORD m_VideoWidth;
	DWORD m_VideoHeight;

private:
    wil::com_ptr<ID3D11Device> pDX11Device;
	HRESULT initD3D11();
	HRESULT initTexture();
	HRESULT EventNotify(DWORD meEvent, DWORD_PTR param1, DWORD param2);
	HRESULT startVideoThread();
	LONGLONG updateFrame();
	void printErrorMessage(DWORD_PTR param1);

	wil::com_ptr<MyPlayerCallback> m_frameCallback;
	std::function<void(bool)> m_loadCallback;

    wil::com_ptr<IMFDXGIDeviceManager> m_pDXGIManager;
	wil::com_ptr<IDXGIAdapter> m_adapter;
    wil::com_ptr<IMFMediaEngine> m_pEngine;
    wil::com_ptr<IMFMediaEngineEx> m_pEngineEx;
	wil::com_ptr<ID3D11Texture2D> m_pTexture;

	std::mutex m_mutex;
	bool m_hasVideo = false;
	LONGLONG m_maxFrameInterval = 10;

	BOOL m_isPlaying = FALSE;
	BOOL m_isEnded = FALSE;
	HANDLE m_playingEvent = NULL; // win32 event
	LONGLONG m_seekingToPts = -1; // seeking pts if seeking
	bool m_isShutdown;

    MFVideoNormalizedRect m_frameRectSrc = {};
    RECT m_frameRectDst = {};
	HANDLE m_threadHandle = NULL;
};