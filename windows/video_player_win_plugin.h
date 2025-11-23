#ifndef FLUTTER_PLUGIN_VIDEO_PLAYER_WIN_PLUGIN_H_
#define FLUTTER_PLUGIN_VIDEO_PLAYER_WIN_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

// Jacky {
#include <windows.h>
#include <map>
#include <mutex>
#include "my_grabber_player.h"
// Jacky }

namespace video_player_win {

class VideoPlayerWinPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  VideoPlayerWinPlugin(flutter::PluginRegistrarWindows *registrar);

  virtual ~VideoPlayerWinPlugin();

  // Disallow copy and assign.
  VideoPlayerWinPlugin(const VideoPlayerWinPlugin&) = delete;
  VideoPlayerWinPlugin& operator=(const VideoPlayerWinPlugin&) = delete;

  std::optional<LRESULT> HandleWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

 private:
   // The ID of the WindowProc delegate registration.
  int window_proc_id = -1;

  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);


  // Jacky {

  void createTexture(MyPlayer* data);
  MyPlayer* getPlayerById(int64_t textureId, bool autoCreate = false);
  void destroyPlayerById(int64_t textureId, bool toRelease);
  void destroyAllPlayers();

public:  
  flutter::PluginRegistrarWindows *m_registrar;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> gMethodChannel;
  flutter::TextureRegistrar* texture_registar_;

  HWND m_nativeHWND;
  std::map<int64_t, MyPlayer*> playerMap; // textureId -> MyPlayerInternal*
  std::mutex m_mapMutex;
  IDXGIAdapter* m_dxgiAdapter;
  // Jacky }      
};

}  // namespace video_player_win

#endif  // FLUTTER_PLUGIN_VIDEO_PLAYER_WIN_PLUGIN_H_
