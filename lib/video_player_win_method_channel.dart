import 'dart:developer';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'video_player_win.dart';
import 'video_player_win_platform_interface.dart';

/// An implementation of [VideoPlayerWinPlatform] that uses method channels.
class MethodChannelVideoPlayerWin extends VideoPlayerWinPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('video_player_win');

  final playerMap = <int, WinVideoPlayerController>{};

  MethodChannelVideoPlayerWin() {
    methodChannel.setMethodCallHandler((call) async {
      //log("[videoplayer] native->flutter: $call");
      int? textureId = call.arguments["textureId"];
      assert(textureId != null);
      final player = playerMap[textureId];
      if (player == null) {
        log("player not found: id = $textureId");
        return;
      }

      if (call.method == "OnPlaybackEvent") {
        int state = call.arguments["state"]!;
        player.onPlaybackEvent_(state);
      } else {
        assert(false, "unknown call from native: ${call.method}");
      }
    });
  }

  @override
  void unregisterPlayer(int textureId) {
    playerMap.remove(textureId);
  }

  @override
  WinVideoPlayerController? getPlayerByTextureId(int textureId) {
    return playerMap[textureId];
  }

  @override
  Future<WinVideoPlayerValue?> openVideo(WinVideoPlayerController player, int textureId, String path) async {
    var arguments = await methodChannel.invokeMethod<Map>('openVideo', { "textureId": -1, "path": path });
    if (arguments == null) return null;
    if (arguments["result"] == false) return null;

    int width = arguments["videoWidth"];
    int height = arguments["videoHeight"];
    double volume = arguments["volume"];
    var value = WinVideoPlayerValue(    
      position: Duration.zero,  
      duration: Duration(milliseconds: arguments["duration"]),
      size: Size(width.toDouble(), height.toDouble()),
      isPlaying: false,
      hasError: false, 
      isInitialized: true,
      volume: volume,
    );
    
    value.textureId = arguments["textureId"];
    playerMap[value.textureId] = player;
    return value;
  }

  @override
  Future<void> play(int textureId) async {
    await methodChannel.invokeMethod<bool>('play', { "textureId": textureId });
  }

  @override
  Future<void> pause(int textureId) async {
    await methodChannel.invokeMethod<bool>('pause', { "textureId": textureId });
  }

  @override
  Future<void> seekTo(int textureId, int ms) async {
    // TODO: will auto play after seek, it seems there is no way to seek without playing in windows media foundation API...
    await methodChannel.invokeMethod<bool>('seekTo', { "textureId": textureId, "ms": ms });
  }

  @override
  Future<int> getCurrentPosition(int textureId) async {
    // TODO: sometimes will return 0 when seeking... seems a bug in windows media foundation API...
    var value = await methodChannel.invokeMethod<int>('getCurrentPosition', { "textureId": textureId });
    return value ?? -1;
  }

  @override
  Future<int> getDuration(int textureId) async {
    var value = await methodChannel.invokeMethod<int>('getDuration', { "textureId": textureId });
    return value ?? -1;
  }

  @override
  Future<void> setPlaybackSpeed(int textureId, double speed) async {
    await methodChannel.invokeMethod<bool>('setPlaybackSpeed', { "textureId": textureId, "speed": speed });
  }

  @override
  Future<void> setVolume(int textureId, double volume) async {
    await methodChannel.invokeMethod<bool>('setVolume', { "textureId": textureId, "volume": volume });
  }

  @override
  Future<void> reset(int textureId) async {
    await methodChannel.invokeMethod<bool>('reset', { "textureId": textureId });
  }

  @override
  Future<void> destroy(int textureId) async {
    await methodChannel.invokeMethod<bool>('destroy', { "textureId": textureId });
  }
}
