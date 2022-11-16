#pragma once

#include <windows.h>
#include <mfidl.h>
#include <mfapi.h>
#include <audiopolicy.h>

#include <wil/com.h>

class MyPlayerCallback
{
public:
	virtual void OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
		LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE* pSampleBuffer,
		DWORD dwSampleSize) = 0;
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
	HRESULT OpenURL(const WCHAR* pszFileName, MyPlayerCallback* callback, HWND hwndVideo = NULL);
	HRESULT Play(LONGLONG ms = -1);
	HRESULT Pause();
	void Shutdown();

	LONGLONG GetDuration();
	LONGLONG GetCurrentPosition();
	HRESULT Seek(LONGLONG ms);
	SIZE GetVideoSize();

	HRESULT SetPlaybackSpeed(float fRate);

	HRESULT GetVolume(float *pVol);
	HRESULT SetVolume(float vol);
	HRESULT SetMute(bool bMute);

	MyPlayer();
	virtual ~MyPlayer();

protected:
	// IMFAsyncCallback implemetation
	HRESULT GetParameters(DWORD* pdwFlags, DWORD* pdwQueue);
	HRESULT Invoke(IMFAsyncResult* pResult);
	virtual void OnPlayerEvent(MediaEventType event) {};

	UINT32 m_VideoWidth;
	UINT32 m_VideoHeight;

private:
	HRESULT initAudioVolume();
	HRESULT CreateTopology(IMFMediaSource* pSource, IMFActivate* pSinkActivate, IMFTopology** ppTopo);

	wil::com_ptr<IMFMediaSession> m_pSession;
	wil::com_ptr<IMFMediaSource> m_pSource;
	wil::com_ptr<ISimpleAudioVolume> m_pSimpleAudioVolume;
	wil::com_ptr<IMFPresentationClock> m_pClock;
	wil::com_ptr<IMFRateControl> m_pRate;
	MFTIME m_hnsDuration;
};