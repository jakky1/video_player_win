#include "include/video_player_win/video_player_win_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "video_player_win_plugin.h"

void VideoPlayerWinPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  video_player_win::VideoPlayerWinPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
