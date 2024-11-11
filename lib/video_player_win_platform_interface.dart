import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'video_player_win.dart';
import 'video_player_win_method_channel.dart';

abstract class VideoPlayerWinPlatform extends PlatformInterface {
  /// Constructs a VideoPlayerWinPlatform.
  VideoPlayerWinPlatform() : super(token: _token);

  static final Object _token = Object();

  static VideoPlayerWinPlatform _instance = MethodChannelVideoPlayerWin();

  /// The default instance of [VideoPlayerWinPlatform] to use.
  ///
  /// Defaults to [MethodChannelVideoPlayerWin].
  static VideoPlayerWinPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [VideoPlayerWinPlatform] when
  /// they register themselves.
  static set instance(VideoPlayerWinPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  void registerPlayer(int textureId, WinVideoPlayerController player) {
    throw UnimplementedError('registerPlayer() has not been implemented.');
  }

  void unregisterPlayer(int textureId) {
    throw UnimplementedError('unregisterPlayer() has not been implemented.');
  }

  WinVideoPlayerController? getPlayerByTextureId(int textureId) {
    throw UnimplementedError(
        'getPlayerByTextureId() has not been implemented.');
  }

  Future<WinVideoPlayerValue?> openVideo(WinVideoPlayerController player,
      int textureId, String path, Map<String, String> httpHeaders) {
    throw UnimplementedError('openVideo() has not been implemented.');
  }

  Future<void> play(int textureId) {
    throw UnimplementedError('play() has not been implemented.');
  }

  Future<void> pause(int textureId) {
    throw UnimplementedError('pause() has not been implemented.');
  }

  Future<void> seekTo(int textureId, int ms) {
    throw UnimplementedError('seekTo() has not been implemented.');
  }

  Future<int> getCurrentPosition(int textureId) {
    throw UnimplementedError('getCurrentPosition() has not been implemented.');
  }

  Future<int?> getDuration(int textureId) {
    throw UnimplementedError('getDuration() has not been implemented.');
  }

  Future<void> setPlaybackSpeed(int textureId, double speed) {
    throw UnimplementedError('setPlaybackSpeed() has not been implemented.');
  }

  Future<void> setVolume(int textureId, double volume) {
    // volume: 0.0 ~ 1.0
    throw UnimplementedError('setVolume() has not been implemented.');
  }

  Future<void> dispose(int textureId) {
    throw UnimplementedError('destroy() has not been implemented.');
  }
}
