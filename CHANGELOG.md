## 3.2.2

* Support multiple window (with `desktop_multi_window` package)

## 3.2.1

* Fix: low FPS on vp8 / vp9 (.webm) video 

## 3.2.0

* support http headers (experimental)
* BREAKING CHANGE: now `WinVideoPlayer` and `VideoPlayer` DO NOT automatically keep aspect ratio, act as `video_player`.
* Fix cmake error in cmake v3.31 or later

## 3.1.2

* Support load from assets.

## 3.1.1

* Fix #44: Fix setLooping(true) will auto play without video frames when play ended

## 3.1.0

* Fix crash issue if Windows not support DirectX 12.

## 3.0.0

* Re-implemented by IMFMediaEngine API of Windows Media Foundation.
* Support .m3u8 now
* Support video scrubbing

## 2.3.11

* Fix @41: can't play video if no audio out device

## 2.3.10

* destroy all old players when hot-restart in debug mode.
* remove black background in WinVideoPlayerWidget.

## 2.3.9

* Fix exception

## 2.3.8

* Fix: unexpected dispose controller when VideoPlayer created without use it, even if it is still referenced.
* Fix: should set value.isInitialized to false while decode error

## 2.3.7

* Fix: crash when open wrong file path or url.

## 2.3.6

* Fix: controller.setPlaybackSpeed() not working

## 2.3.5

* update README: no need to call registerWith()

## 2.3.4

* Fix: getCurrentPosition() shouldn't return 0 during seeking
* support controller.value.isCompleted
* auto call dispose() when controller garbage-collected

## 2.3.2

* Fix: sometimes crash when closing player

## 2.3.1

* Fix: cannot play video path containing non-ASCII characters

## 2.3.0
* Fix: get player position directly from windows media foundation, not cached value.
* support Dart 3.0

## 2.2.2

* Fix: shouldn't auto play when seeking in pause state
* Fix: show the first frame when video loaded and Play() not called
* Fix: prevent video freeze when call Play() twice
* keep screen on (disable screensaver) while playing video.

## 2.2.0

* Enhance performance
* Fix memory leak
* Fix SetVolume() issue

## 2.0.0

* Enable GPU hardware acceleration

## 1.1.6

* Support non-English filename

## 1.1.5

* Fix video skew when width or height is not multiple of 16

## 1.1.4

* Fix crash when dispose video which has no audio or video stream

## 1.1.3

* support AV1 video
* enhance playback performance

## 1.1.2

* fix compile error

## 1.1.1

* change sdk version limitation

## 1.1.0

* fix crash issue
* fix memory leak

## 1.0.4

* modify README.md

## 1.0.3

* modify README.md

## 1.0.2

* Fix SetLooping()
* Fix bug when play to end

## 1.0.1

* Fix memory leak in cpp code

## 1.0.0

* Initial version
