#pragma once

#include <windows.h>
#include <mfidl.h>
#include <mfapi.h>
#include <audiopolicy.h>
#include <functional>
#include <mutex>

#include <wil/com.h>

#include "DX11VideoRenderer/DX11VideoRenderer.h"

class MyPlayerCallback : public IUnknown
{
public:
#ifdef WIN32
	virtual void OnProcessFrame(ID3D11Texture2D* texture) = 0;
#else
	virtual void OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
		LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE* pSampleBuffer,
		DWORD dwSampleSize) = 0;
#endif
};

class MyPlayer : public IMFAsyncCallback
{
public:
	inline STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
		return S_OK;
	}
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

	MyPlayer();
	virtual ~MyPlayer();

protected:
	// IMFAsyncCallback implemetation
	HRESULT GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
	HRESULT Invoke(IMFAsyncResult* pResult);
	virtual void OnPlayerEvent(MediaEventType event) {};
	HRESULT CreateMediaSourceAsync(PCWSTR pszURL, std::function<void(IMFMediaSource* pSource)> callback);

	std::mutex m_mutex;
	UINT32 m_VideoWidth;
	UINT32 m_VideoHeight;

private:
	HRESULT initAudioVolume();
	HRESULT CreateTopology(IMFMediaSource* pSource, IMFActivate* pSinkActivate, IMFTopology** ppTopo);
	void cancelAsyncLoad();
	HRESULT doSetVolume(float fVol);

	wil::com_ptr<IMFMediaSession> m_pSession;
	wil::com_ptr<IMFMediaSource> m_pMediaSource;
	wil::com_ptr<IMFActivate> m_pVideoTransformActivate;
	wil::com_ptr<IMFActivate> m_pVideoSinkActivate;
	wil::com_ptr<IMFActivate> m_pAudioRendererActivate;
	wil::com_ptr<IMFAudioStreamVolume> m_pAudioVolume;
	wil::com_ptr<IMFPresentationClock> m_pClock;
	wil::com_ptr<IMFRateControl> m_pRate;
	wil::com_ptr<IMFSourceResolver> m_pSourceResolver;
	wil::com_ptr<IUnknown> m_pSourceResolverCancelCookie;
	MFTIME m_hnsDuration;
	bool m_isShutdown;
	HWND m_ChildWnd;

	float m_vol;
	bool m_isUserAskPlaying;

	// save parameters in OpenURL(), used to re-open when open failed
	bool m_topoSet = false; // is topology set succesfully
	std::function<void(void)> m_reopenFunc;
	std::function<void(bool)> m_loadCallback;
	//
};