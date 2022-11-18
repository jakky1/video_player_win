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

#include <chrono>
flutter::MethodChannel<flutter::EncodableValue>* gMethodChannel = NULL;

inline uint64_t getCurrentTime() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

flutter::TextureRegistrar* texture_registar_ = NULL;

class MyPlayerInternal : public MyPlayer, public MyPlayerCallback {
public:
  int64_t textureId = -1;
  FlutterDesktopPixelBuffer pixel_buffer;

  MyPlayerInternal() {}
  ~MyPlayerInternal() {
    if (m_pBuffer != NULL) delete m_pBuffer;
    m_pBuffer = NULL;
  }

private:
  uint64_t lastFrameTime = 0;
  enum PlaybackState { IDLE = 0, BUFFERING_START, BUFFERING_END, START, PAUSE, STOP, END, SESSION_ERROR };
  PlaybackState mPlaybackState = IDLE;
  BYTE* m_pBuffer = NULL;
  DWORD m_lastSampleSize = 0;

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
        break;
      case MESessionPaused:
        mPlaybackState = PAUSE;
        break;
      case MESessionStopped:
        mPlaybackState = STOP;
        break;
      case MESessionClosed:
        mPlaybackState = IDLE;
        break;
      case MESessionEnded:
        mPlaybackState = END;
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

  void OnProcessSample(REFGUID guidMajorMediaType, DWORD dwSampleFlags,
      LONGLONG llSampleTime, LONGLONG llSampleDuration, const BYTE* pSampleBuffer,
      DWORD dwSampleSize) 
  {
      if (textureId == -1) return; //player maybe shutdown or deleted
      uint64_t now = getCurrentTime();
      if (now - lastFrameTime < 30) return;
      lastFrameTime = now;

      if (m_lastSampleSize != dwSampleSize) {
        m_lastSampleSize = dwSampleSize;
        if (m_pBuffer != NULL) delete m_pBuffer;
        m_pBuffer = new BYTE[m_lastSampleSize];

        pixel_buffer.width = m_VideoWidth;
        pixel_buffer.height = m_VideoHeight;
        pixel_buffer.buffer = m_pBuffer;
      } else if (pixel_buffer.width != m_VideoWidth) {
        // ex. video 1280x720 -> 720x1280, sample size won't change, but we need set the correct resolution
        pixel_buffer.width = m_VideoWidth;
        pixel_buffer.height = m_VideoHeight;
      }

      const BYTE *pSrc = pSampleBuffer + 2;
      BYTE *pDst = m_pBuffer;
      BYTE *pDstEnd = pDst + dwSampleSize;
      do {
        if (textureId == -1) break;
        *(pDst++) = *(pSrc--);
        *(pDst++) = *(pSrc--);
        *(pDst) = *(pSrc);

        pDst += 2;
        pSrc += (4 + 2);
      } while (pDst < pDstEnd);

      if (texture_registar_ != NULL && textureId != -1) {
        texture_registar_->MarkTextureFrameAvailable(textureId);
      }
  }
};

std::map<int64_t, MyPlayerInternal*> playerMap; // textureId -> MyPlayerInternal*
bool isMFInited = false;

void createTexture(MyPlayerInternal* data) {
  memset(&data->pixel_buffer, 0, sizeof(data->pixel_buffer));
  flutter::TextureVariant* texture = new flutter::TextureVariant(flutter::PixelBufferTexture(
    [=](size_t width, size_t height) -> const FlutterDesktopPixelBuffer* {
      return &data->pixel_buffer;
    }));
  data->textureId = texture_registar_->RegisterTexture(texture);
}

MyPlayerInternal* getPlayerById(int64_t textureId, bool autoCreate = false) {
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
  MyPlayerInternal* data = playerMap[textureId];
  if (data == NULL) return;
  playerMap.erase(textureId);
  if (data->textureId != -1) {
    texture_registar_->UnregisterTexture(data->textureId);
    data->textureId = -1;
  }
  
  // NOTE: because m_pSession->BeginGetEvent(this) will keep *this (player), 
  //       so we need to call m_pSession->Shutdown() first
  //       then client call player->Release() will make refCount = 0
  data->Shutdown();
  data->Release();
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
}

VideoPlayerWinPlugin::VideoPlayerWinPlugin() {}

VideoPlayerWinPlugin::~VideoPlayerWinPlugin() {
  texture_registar_ = NULL; //Jacky
  MFShutdown();
}
void VideoPlayerWinPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

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

    textureId = player->textureId;
    std::shared_ptr<flutter::MethodResult<flutter::EncodableValue>> shared_result = std::move(result);
    HRESULT hr = player->OpenURL(std::wstring(path.begin(), path.end()).c_str(), player, NULL, [=](bool isSuccess) {
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
  } else if (method_call.method_name().compare("reset") == 0) {
    player->Shutdown();    
    result->Success(flutter::EncodableValue(true));
  } else if (method_call.method_name().compare("destroy") == 0) {
    destroyPlayerById(textureId);
    result->Success(flutter::EncodableValue(true));
  } else {
    result->NotImplemented();
  }
}

}  // namespace video_player_win
