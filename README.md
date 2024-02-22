# video_player_win

[visits-count-image]: https://img.shields.io/badge/dynamic/json?label=Visits%20Count&query=value&url=https://api.countapi.xyz/hit/jakky1_video_player_win/visits

Flutter video player for Windows, lightweight, using Windows built-in Media Foundation API.
Windows implementation of the [video_player][1] plugin.

## Platform Support

This package itself support only Windows.

But use it with [video_player][1], your app can support Windows / Android / iOS / Web at the same time.

## Built-in control panel & Fullscreen & Subtitle support

Please use package [video_player_control_panel][2] instead.

Which also use this package to play video on Windows.

## Features & Limitations

Features:

- GPU hardware acceleration, low CPU usage *(maybe support 4K 60fps video?)*
- No GPL / LGPL 3rd-party libraries inside.
- Only one dll file (~180 KB) added as a plugin.
- Support Windows / Android / iOS / Web by collaboration with [video_player][1]

Limitations:
- This package doesn't support HLS (.m3u8)


But, since this package use Microsoft Media Foundation API, there are some limtations:

- Playback will use codecs preloaded in Windows OS. If you want to play some video format that not supported by these preloaded codecs, you need to install 3rd-party codecs exe file, about 18 MB. (see the next section).

## Supported Formats in Windows (Important !)

Ref: [Windows preloaded codec list][8]

This package use Windows built-in Media Foundation API.
So playback video will use the codecs preload in your Windows environment.
All the media format can be played by WMP (Windows Media Player) can also be played by this package.
However, the preloaded codecs in Windows is limited.

If you have a media file cannot played by this package, ALSO CANNOT played by WMP  (Windows Media Player), it means that this media file is not support by codecs preloaded in your Windows.

In this case, please install ONE of the following codec pack into your Windows:
- [K-Lite codec pack][3] (~18MB)
- [Windows 10 Codec Pack][4]

You can auto-install codec by the following Dart code:
```
import 'dart:io';

Process.run('E:\\K-Lite_Codec_Pack_1730_Basic.exe', ['/silent']).then((value) {
  if (value.exitCode == 0) log("installation success");
  else log("installation failed");
});
```

After install the codec pack, most of the media format are supported.

## Supported AV1 video

To play AV1 video,
- install codec in [Microsoft Store][7].
- or download the [AV1 codec installer][6] (only 850 KB)

You can silently auto-install codec by the following Dart code:
```
import 'dart:io';

Process.run('powershell', ['Add-AppxPackage', '-Path', 'E:\\av1-video-extension-1-1-52851-0.appx']).then((value) {
  if (value.exitCode == 0) log("installation success");
  else log("installation failed");
});
```

# Problem shootting for building fail

If you build fail with this package, and the error message has the keyword "**MSB3073**":

- run "**flutter build windows**" in command line in [**Administrator**] mode


# Quick Start

## Installation

Add this to your package's `pubspec.yaml` file:

```yaml
dependencies:
  video_player: ^2.5.1
  video_player_win: ^2.0.0
```

Or

```yaml
dependencies:
  video_player: ^2.5.1
  video_player_win:
    git:
      url: https://github.com/jakky1/video_player_win.git
      ref: master
```

# Usage

## video / audio playback

Play from network source:
```
var controller = VideoPlayerController.network("https://www.your-web.com/sample.mp4");
controller.initialize().then((value) {
  if (controller.value.isInitialized) {
    controller.play();
  } else {
    log("video file load failed");
  }
}).catchError((e) {
  log("controller.initialize() error occurs: $e");
});
```

Play from file:
```
var controller = VideoPlayerController.file(File("E:\\test.mp4"));
```

If the file is a video, build a display widget to show video frames:
```
Widget build(BuildContext context) {
  return VideoPlayer(controller);
}
```

# operations

- Play: ``` controller.play(); ```
- Pause: ``` controller.pause(); ```
- Seek: ``` controller.seekTo( Duration(minute: 10, second:30) ); ```
- set playback speed: (normal speed: 1.0)
``` controller.setPlaybackSpeed(1.5); ```
- set volume: (max: 1.0 , mute: 0.0)
``` controller.setVolume(0.5); ```
- set looping:  ``` controller.setLooping(true); ```
- free resource: ``` controller.dispose(); ```

# Listen playback events and values
```
void onPlaybackEvent() {
	final value = controller.value;
	// value.isInitialized (bool)
	// value.size (Size, video size)
	// value.duration (Duration)
	// value.isPlaying (bool)
	// value.isBuffering (bool)
	// value.isCompleted (bool)
	// value.position (Duration)
}
controller.addListener(onPlaybackEvent);
...
controller.removeListener(onPlaybackEvent); // remember to removeListener()
```
## Release resource
```
controller.dispose();
```

# standalone mode

If your app only runs on Windows, and you want to remove library dependencies as many as possible, you can modify `pubspec.yaml` file:

```yaml
dependencies:
  # video_player: ^2.4.7 # mark this line, for Windows only app
  video_player_win: ^1.0.1
```

and modify all the following class name in your code:
```
VideoPlayer -> WinVideoPlayer  // add "Win" prefix
VideoPlayerController -> WinVideoPlayerController  // add "Win" prefix
```

just only modify class names. All the properties / method are the same with [video_player][1]


# Example

```
import 'dart:developer';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:video_player_win/video_player_win.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({Key? key}) : super(key: key);

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {

  late VideoPlayerController controller;

  @override
  void initState() {
    super.initState();
    controller = VideoPlayerController.file(File("E:\\test_youtube.mp4"));
    controller.initialize().then((value) {
      if (controller.value.isInitialized) {
        controller.play();
        setState(() {});
      } else {
        log("video file load failed");
      }
    });
  }

  @override
  void dispose() {
    super.dispose();
    controller.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('video_player_win example app'),
        ),

        body: Stack(children: [
          VideoPlayer(controller),
          Positioned(
            bottom: 0,
            child: Column(children: [
              ValueListenableBuilder(
                valueListenable: controller,
                builder: ((context, value, child) {
                  int minute = controller.value.position.inMinutes;
                  int second = controller.value.position.inSeconds % 60;
                  return Text("$minute:$second", style: Theme.of(context).textTheme.headline6!.copyWith(color: Colors.white, backgroundColor: Colors.black54));
                }),
              ),
              ElevatedButton(onPressed: () => controller.play(), child: const Text("Play")),
              ElevatedButton(onPressed: () => controller.pause(), child: const Text("Pause")),
              ElevatedButton(onPressed: () => controller.seekTo(Duration(milliseconds: controller.value.position.inMilliseconds+ 10*1000)), child: const Text("Forward")),
            ])),
        ]),
      ),
    );
  }
}
```
[1]: https://pub.dev/packages/video_player "video_player"
[2]: https://pub.dev/packages/video_player_control_panel "video_player_control_panel"
[3]: https://codecguide.com/ "K-Lite Codec Pack"
[4]: https://www.windows10codecpack.com/ "Windows 10 Codec Pack"
[5]: https://pub.dev/packages/webview_win_floating "webview_win_floating"
[6]: https://av1-video-extension.en.uptodown.com/windows "AV1 codec installer"
[7]: https://apps.microsoft.com/store/detail/av1-video-extension/9MVZQVXJBQ9V?hl=en-us&gl=us "Microsoft Store AV1 codec"
[8]: https://learn.microsoft.com/en-us/windows/win32/medfound/supported-media-formats-in-media-foundation "Windows preloaded codec list"