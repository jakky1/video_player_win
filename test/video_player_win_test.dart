/*
import 'package:flutter_test/flutter_test.dart';
import 'package:video_player_win/video_player_win.dart';
import 'package:video_player_win/video_player_win_platform_interface.dart';
import 'package:video_player_win/video_player_win_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockVideoPlayerWinPlatform 
    with MockPlatformInterfaceMixin
    implements VideoPlayerWinPlatform {

  @override
  Future<void> destroy(int playerId) {
    throw UnimplementedError();
  }

  @override
  Future<int?> openVideo(WinVideoPlayerController player, int playerId, String path) {
    throw UnimplementedError();
  }

  @override
  Future<void> pause(int playerId) {
    throw UnimplementedError();
  }

  @override
  Future<void> play(int playerId) {
    throw UnimplementedError();
  }

  @override
  void registerPlayer(int playerId, WinVideoPlayerController player) {
  }

  @override
  Future<void> reset(int playerId) {
    throw UnimplementedError();
  }

  @override
  Future<void> seekTo(int playerId, int ms) {
    throw UnimplementedError();
  }

  @override
  Future<void> setPlaybackSpeed(int playerId, double rate) {
    throw UnimplementedError();
  }

  @override
  void unregisterPlayer(int playerId) {
  }
}

void main() {
  final VideoPlayerWinPlatform initialPlatform = VideoPlayerWinPlatform.instance;

  test('$MethodChannelVideoPlayerWin is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelVideoPlayerWin>());
  });

  test('getPlatformVersion', () async {
    //VideoPlayerWin videoPlayerWinPlugin = VideoPlayerWin();
    MockVideoPlayerWinPlatform fakePlatform = MockVideoPlayerWinPlatform();
    VideoPlayerWinPlatform.instance = fakePlatform;
  
    //expect(await videoPlayerWinPlugin.getPlatformVersion(), '42');
  });
}
*/