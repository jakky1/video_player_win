#ifndef FLUTTER_PLUGIN_VIDEO_PLAYER_WIN_PLUGIN_H_
#define FLUTTER_PLUGIN_VIDEO_PLAYER_WIN_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>

#include <memory>

namespace video_player_win {

class VideoPlayerWinPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar);

  VideoPlayerWinPlugin();

  virtual ~VideoPlayerWinPlugin();

  // Disallow copy and assign.
  VideoPlayerWinPlugin(const VideoPlayerWinPlugin&) = delete;
  VideoPlayerWinPlugin& operator=(const VideoPlayerWinPlugin&) = delete;

 private:
  // Called when a method is called on this plugin's channel from Dart.
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

}  // namespace video_player_win

#endif  // FLUTTER_PLUGIN_VIDEO_PLAYER_WIN_PLUGIN_H_
