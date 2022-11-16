import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
//import 'package:video_player_win/video_player_win_method_channel.dart';

void main() {
  //MethodChannelVideoPlayerWin platform = MethodChannelVideoPlayerWin();
  const MethodChannel channel = MethodChannel('video_player_win');

  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    channel.setMockMethodCallHandler((MethodCall methodCall) async {
      return '42';
    });
  });

  tearDown(() {
    channel.setMockMethodCallHandler(null);
  });

  test('getPlatformVersion', () async {
    //expect(await platform.getPlatformVersion(), '42');
  });
}
