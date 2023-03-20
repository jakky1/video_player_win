#include "video_player_win_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <sstream>

#include "my_grabber_player.h"
#include <mfapi.h>
#include <Shlwapi.h>
#include <stdio.h>

// Jacky {
flutter::PluginRegistrarWindows *g_registrar; // Jacky

#include <stack>
#include <set>

#include <chrono>
flutter::MethodChannel<flutter::EncodableValue>* gMethodChannel = NULL;

inline uint64_t getCurrentTime() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

#include <set>
class ScreenOnKeeper {
public:
  ScreenOnKeeper() {
    m_id = ++g_lastId;

    if (g_handle == 0) {
      REASON_CONTEXT ctx;
      ctx.Version = POWER_REQUEST_CONTEXT_VERSION;
      ctx.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
      ctx.Reason.SimpleReasonString = L"[video_player_win]";
      g_handle = PowerCreateRequest(&ctx);
    }
  }

  ~ScreenOnKeeper() {
    enable(false);
  }

  void enable(bool b) {
    if (m_isEnabled == b)
      return;
    m_isEnabled = b;
    const std::lock_guard<std::mutex> lock(g_keeperMutex);

    bool exists = g_keeperIdSet.find(m_id) != g_keeperIdSet.end();

    if (b) {
      if (!exists) {
        if (g_keeperIdSet.empty()) {
          PowerSetRequest(g_handle, PowerRequestSystemRequired);
          PowerSetRequest(g_handle, PowerRequestDisplayRequired);
          //std::cout << "[video_player_win] enable keep screen on" << std::endl;
        }
        g_keeperIdSet.insert(m_id);
      }
    } else {
      if (exists) {
        g_keeperIdSet.erase(m_id);
        if (g_keeperIdSet.empty()) {
          PowerClearRequest(g_handle, PowerRequestSystemRequired);
          PowerClearRequest(g_handle, PowerRequestDisplayRequired);
          //std::cout << "[video_player_win] disable keep screen on" << std::endl;
        }
      }
    }
  }
private:
  bool m_isEnabled = false;
  int m_id;

  static int g_lastId;
  static std::set<int> g_keeperIdSet;
  static std::mutex g_keeperMutex;
  static HANDLE g_handle;
};
int ScreenOnKeeper::g_lastId = 0;
std::set<int> ScreenOnKeeper::g_keeperIdSet;
std::mutex ScreenOnKeeper::g_keeperMutex;
HANDLE ScreenOnKeeper::g_handle = 0;


flutter::TextureRegistrar* texture_registar_ = NULL;

class MyPlayerInternal : public MyPlayer, public MyPlayerCallback {
public:
  int64_t textureId = -1;
  FlutterDesktopGpuSurfaceDescriptor texture_buffer;
  HWND mChildHWND = 0;

  MyPlayerInternal() {}
  ~MyPlayerInternal() {
    keepScreenOn(false);
    textureId = -1;
  }

	inline STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
    return MyPlayer::QueryInterface(riid, ppv);
	}
	inline STDMETHODIMP_(ULONG) AddRef() {
		return MyPlayer::AddRef();
	}
	STDMETHODIMP_(ULONG) Release() {
		return MyPlayer::Release();
	}

private:
  bool mTextureInited = false;
  HANDLE mSharedTextureHandle = 0;
  ScreenOnKeeper m_screenOnKeeper;

  enum PlaybackState { IDLE = 0, BUFFERING_START, BUFFERING_END, START, PAUSE, STOP, END, SESSION_ERROR };
  PlaybackState mPlaybackState = IDLE;

  void keepScreenOn(bool keepOn) {
    if (m_VideoWidth <= 0) // if audio-only media
      return;
    m_screenOnKeeper.enable(keepOn);
  }

  void OnPlayerEvent(MediaEventType event) override
  {
    switch (event) {
      case MEBufferingStarted:
        mPlaybackState = BUFFERING_START;
        break;
      case MEBufferingStopped:
        mPlaybackState = BUFFERING_END;
        break;
      case MESessionStarted:
        mPlaybackState = START;
        keepScreenOn(true);
        break;
      case MESessionPaused:
        mPlaybackState = PAUSE;
        keepScreenOn(false);
        break;
      case MESessionStopped:
        mPlaybackState = STOP;
        keepScreenOn(false);
        break;
      case MESessionClosed:
        mPlaybackState = IDLE;
        keepScreenOn(false);
        break;
      case MESessionEnded:
        mPlaybackState = END;
        keepScreenOn(false);
        break;
      case MEError:
        mPlaybackState = SESSION_ERROR;
        break;
      default:
        return;
    }

    flutter::EncodableMap arguments;
    arguments[flutter::EncodableValue("textureId")] = flutter::EncodableValue(textureId);
    arguments[flutter::EncodableValue("state")] = flutter::EncodableValue(mPlaybackState);
    gMethodChannel->InvokeMethod("OnPlaybackEvent", std::make_unique<flutter::EncodableValue>(arguments));
  }

  void initTexture(ID3D11Texture2D* texture)
  {
    if (!mTextureInited)
    {
      HRESULT hr;
      D3D11_TEXTURE2D_DESC desc;
      texture->GetDesc(&desc);

      wil::com_ptr<IDXGIResource1> resource;
      texture->QueryInterface(IID_PPV_ARGS(&resource));
      hr = resource->GetSharedHandle(&mSharedTextureHandle);
      if (!SUCCEEDED(hr)) {
        std::cout << "[video_player_win] native GetSharedHandle failed: " << hr << std::endl;
        return;
      }

      texture_buffer.struct_size = sizeof(FlutterDesktopGpuSurfaceDescriptor);
      texture_buffer.width = desc.Width;
      texture_buffer.height = desc.Height;
      texture_buffer.format = kFlutterDesktopPixelFormatBGRA8888;  //kFlutterDesktopPixelFormatRGBA8888; //or kFlutterDesktopPixelFormatBGRA8888
      texture_buffer.handle = mSharedTextureHandle;

      mTextureInited = true;
    }
  }

  void OnProcessFrame(ID3D11Texture2D* texture)
  {
    if (texture_registar_ != NULL && textureId != -1) {
      initTexture(texture);
      texture_registar_->MarkTextureFrameAvailable(textureId);
    }
  }
};

std::map<int64_t, MyPlayerInternal*> playerMap; // textureId -> MyPlayerInternal*
std::mutex mapMutex;
bool isMFInited = false;

void createTexture(MyPlayerInternal* data) {
  memset(&data->texture_buffer, 0, sizeof(data->texture_buffer));

  flutter::TextureVariant* texture = new flutter::TextureVariant(flutter::GpuSurfaceTexture(
    kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle,
    [=](size_t width, size_t height) -> const FlutterDesktopGpuSurfaceDescriptor* {
      return &data->texture_buffer;
    }));
  data->textureId = texture_registar_->RegisterTexture(texture);
}

MyPlayerInternal* getPlayerById(int64_t textureId, bool autoCreate = false) {
  std::lock_guard<std::mutex> lock(mapMutex);
  MyPlayerInternal* data = playerMap[textureId];
  if (data == NULL && autoCreate) {
    if (!isMFInited) {
      MFStartup(MF_VERSION); //TODO: hint user if startup failed... if it is possible?
      isMFInited = true;
    }
    data = new MyPlayerInternal();
    createTexture(data);
    playerMap[data->textureId] = data;
  }
  return data;
}

void destroyPlayerById(int64_t textureId) {
  std::lock_guard<std::mutex> lock(mapMutex);
  MyPlayerInternal* data = playerMap[textureId];
  if (data == NULL) return;
  playerMap.erase(textureId);
  if (data->textureId != -1) {
    texture_registar_->UnregisterTexture(data->textureId);
    data->textureId = -1;
  }

  data->Release();
  //std::cout << "native destroy player id: " << textureId << std::endl;
}

// Jacky }

namespace video_player_win {

// static
void VideoPlayerWinPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "video_player_win",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<VideoPlayerWinPlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));

  texture_registar_ = registrar->texture_registrar(); //Jacky
  gMethodChannel = new flutter::MethodChannel<flutter::EncodableValue>(registrar->messenger(), "video_player_win",
          &flutter::StandardMethodCodec::GetInstance()); //Jacky

  g_registrar = registrar; //Jacky
}

VideoPlayerWinPlugin::VideoPlayerWinPlugin() {}

VideoPlayerWinPlugin::~VideoPlayerWinPlugin() {
  texture_registar_ = NULL; //Jacky
  MFShutdown();
}

void VideoPlayerWinPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

  //std::cout << "HandleMethodCall: " << method_call.method_name() << std::endl;
  flutter::EncodableMap arguments = std::get<flutter::EncodableMap>(*method_call.arguments());

  auto textureId = arguments[flutter::EncodableValue("textureId")].LongValue();
  MyPlayerInternal* player;
  bool isOpenVideo = method_call.method_name().compare("openVideo") == 0;
  if (isOpenVideo) {
    player = getPlayerById(-1, true);
  } else {
    player = getPlayerById(textureId, false);
  }
  if (player == nullptr) {
    result->Success();
    return;
  }

  if (isOpenVideo) {
    auto path = std::get<std::string>(arguments[flutter::EncodableValue("path")]);
    WCHAR wPath[1024];
    auto convResult = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wPath, sizeof(wPath) / sizeof(WCHAR));
    if (convResult < 0) {
      std::cout << "[video_player_win] native convert path to utf16 (WCHAR*) failed: path = " << path << std::endl;
    }

    textureId = player->textureId;
    std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> shared_result = std::move(result);
    HWND hwnd = GetAncestor(g_registrar->GetView()->GetNativeWindow(), GA_ROOT);
    HRESULT hr = player->OpenURL(wPath, player, hwnd, [=](bool isSuccess) {
      if (isSuccess) {
        auto _player = getPlayerById(textureId, false);
        if (_player == NULL) {
          // the player is disposed between async OpenURL() and callback here
          flutter::EncodableMap map;
          map[flutter::EncodableValue("result")] = flutter::EncodableValue(false);
          shared_result->Success(map);
          return;
        }

        SIZE videoSize = _player->GetVideoSize();
        flutter::EncodableMap map;
        float volume = 1.0f;
        _player->GetVolume(&volume);
        map[flutter::EncodableValue("result")] = flutter::EncodableValue(true);
        map[flutter::EncodableValue("textureId")] = flutter::EncodableValue(_player->textureId);
        map[flutter::EncodableValue("duration")] = flutter::EncodableValue((int64_t)_player->GetDuration());
        map[flutter::EncodableValue("videoWidth")] = flutter::EncodableValue(videoSize.cx);
        map[flutter::EncodableValue("videoHeight")] = flutter::EncodableValue(videoSize.cy);
        map[flutter::EncodableValue("volume")] = flutter::EncodableValue((double)volume);
        shared_result->Success(flutter::EncodableValue(map));
      } else {
        destroyPlayerById(player->textureId);
        flutter::EncodableMap map;
        map[flutter::EncodableValue("result")] = flutter::EncodableValue(false);
        shared_result->Success(map);
      }
    });
    if (FAILED(hr)) {
      flutter::EncodableMap map;
      map[flutter::EncodableValue("result")] = flutter::EncodableValue(false);
      result->Success(map);
    }
  } else if (method_call.method_name().compare("play") == 0) {
    player->Play();
    result->Success(flutter::EncodableValue(true));
  } else if (method_call.method_name().compare("pause") == 0) {
    player->Pause();
    result->Success(flutter::EncodableValue(true));
  } else if (method_call.method_name().compare("seekTo") == 0) {
    auto ms = std::get<int32_t>(arguments[flutter::EncodableValue("ms")]);
    player->Seek(ms);
    result->Success(flutter::EncodableValue(true));
  } else if (method_call.method_name().compare("getCurrentPosition") == 0) {
    long ms = (long) player->GetCurrentPosition();
    result->Success(flutter::EncodableValue(ms));
  } else if (method_call.method_name().compare("getDuration") == 0) {
    long ms = (long) player->GetDuration();
    result->Success(flutter::EncodableValue(ms));
  } else if (method_call.method_name().compare("setPlaybackSpeed") == 0) {
    double speed = std::get<double>(arguments[flutter::EncodableValue("speed")]);
    player->SetPlaybackSpeed((float)speed);
    result->Success(flutter::EncodableValue(true));
  } else if (method_call.method_name().compare("setVolume") == 0) {
    double volume = std::get<double>(arguments[flutter::EncodableValue("volume")]);
    player->SetVolume((float)volume);
    result->Success(flutter::EncodableValue(true));
  } else if (method_call.method_name().compare("shutdown") == 0) {
    // NOTE: because m_pSession->BeginGetEvent(this) will keep *this (player),
    //       so we need to call m_pSession->Shutdown() first
    //       then client call player->Release() will make refCount = 0
    player->Shutdown();
    result->Success(flutter::EncodableValue(true));
  } else if (method_call.method_name().compare("dispose") == 0) {
    destroyPlayerById(textureId);
    result->Success(flutter::EncodableValue(true));
  } else {
    result->NotImplemented();
  }
}

}  // namespace video_player_win
