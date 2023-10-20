export 'video_player_win_plugin.dart';
import 'dart:async';
import 'dart:developer';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:video_player_platform_interface/video_player_platform_interface.dart';
import 'video_player_win_platform_interface.dart';

enum WinDataSourceType { asset, network, file, contentUri }

class WinVideoPlayerValue {
  final Duration duration;
  final bool hasError;
  final bool isBuffering;
  final bool isInitialized;
  final bool isLooping;
  final bool isPlaying;
  final bool isCompleted;
  final double playbackSpeed;
  final Duration position;
  final Size size;
  final double volume;

  int textureId = -1; //for internal use only

  double get aspectRatio => size.isEmpty ? 1 : size.width / size.height;

  WinVideoPlayerValue({
    this.duration = Duration.zero,
    this.hasError = false,
    this.size = Size.zero,
    this.position = Duration.zero,
    //Caption caption = Caption.none,
    //Duration captionOffset = Duration.zero,
    //List<DurationRange> buffered = const <DurationRange>[],
    this.isInitialized = false,
    this.isPlaying = false,
    this.isLooping = false,
    this.isBuffering = false,
    this.isCompleted = false,
    this.volume = 1.0,
    this.playbackSpeed = 1.0,
    //int rotationCorrection = 0,
    //String? errorDescription
  });

  WinVideoPlayerValue copyWith({
    Duration? duration,
    bool? hasError,
    bool? isBuffering,
    bool? isInitialized,
    bool? isLooping,
    bool? isPlaying,
    bool? isCompleted,
    double? playbackSpeed,
    Duration? position,
    Size? size,
    double? volume,
  }) {
    return WinVideoPlayerValue(
      duration: duration ?? this.duration,
      hasError: hasError ?? this.hasError,
      isBuffering: isBuffering ?? this.isBuffering,
      isInitialized: isInitialized ?? this.isInitialized,
      isLooping: isLooping ?? this.isLooping,
      isPlaying: isPlaying ?? this.isPlaying,
      isCompleted: isCompleted ?? this.isCompleted,
      playbackSpeed: playbackSpeed ?? this.playbackSpeed,
      position: position ?? this.position,
      size: size ?? this.size,
      volume: volume ?? this.volume,
    );
  }
}

class WinVideoPlayerController extends ValueNotifier<WinVideoPlayerValue> {
  int textureId_ = -1;
  final String dataSource;
  late final WinDataSourceType dataSourceType;
  bool _isLooping = false;

  final _eventStreamController = StreamController<
      VideoEvent>(); // used for flutter official "video_player" package
  Stream<VideoEvent> get videoEventStream => _eventStreamController.stream;

  Future<Duration?> get position async {
    var pos = await _getCurrentPosition();
    return Duration(milliseconds: pos);
  }

  WinVideoPlayerController._(this.dataSource, this.dataSourceType)
      : super(WinVideoPlayerValue()) {
    if (dataSourceType == WinDataSourceType.contentUri) {
      throw UnsupportedError(
          "VideoPlayerController.contentUri() not supported in Windows");
    }
    if (dataSourceType == WinDataSourceType.asset) {
      throw UnsupportedError(
          "VideoPlayerController.asset() not implement yet.");
    }

    _finalizer.attach(this, this, detach: this);
    //VideoPlayerWinPlatform.instance.registerPlayer(_textureId, this);
  }
  static final Finalizer<WinVideoPlayerController> _finalizer =
      Finalizer((player) {
    // TODO: this finalizer seems not called...
    player.dispose();
    VideoPlayerWinPlatform.instance.unregisterPlayer(player.textureId_);
  });

  WinVideoPlayerController.file(File file)
      : this._(file.path, WinDataSourceType.file);
  WinVideoPlayerController.network(String dataSource)
      : this._(dataSource, WinDataSourceType.network);
  WinVideoPlayerController.asset(String dataSource, {String? package})
      : this._(dataSource, WinDataSourceType.asset);
  WinVideoPlayerController.contentUri(Uri contentUri)
      : this._("", WinDataSourceType.contentUri);

  int _lastSeekId =
      0; // +1 every time when seekTo() called, to cancel all getCurrentPosition() called before seekTo()
  void _cancelTrackingPosition() => _lastSeekId++;
  void _startTrackingPosition(int seekId) async {
    // update position value for every 1000 ms
    int delay = 1000 - (value.position.inMilliseconds % 1000);
    delay = delay ~/ value.playbackSpeed + 1;
    await Future.delayed(Duration(milliseconds: delay));

    // NOTE: DO NOT call getCurrentPosition() during seeking, because windows will return 0...
    if (textureId_ == -1 ||
        !value.isInitialized ||
        !value.isPlaying ||
        (seekId != _lastSeekId)) return;

    var pos = await _getCurrentPosition();
    if (pos < 0 ||
        textureId_ == -1 ||
        !value.isInitialized ||
        (seekId != _lastSeekId)) return;
    value = value.copyWith(position: Duration(milliseconds: pos));

    _startTrackingPosition(seekId);
  }

  void onPlaybackEvent_(int state) {
    switch (state) {
      // MediaEventType in win32 api
      case 1: // MEBufferingStarted
        log("[video_player_win] playback event: buffering start");
        value = value.copyWith(isInitialized: true, isBuffering: true);
        _eventStreamController
            .add(VideoEvent(eventType: VideoEventType.bufferingStart));
        break;
      case 2: // MEBufferingStopped
        log("[video_player_win] playback event: buffering finish");
        value = value.copyWith(isInitialized: true, isBuffering: false);
        _eventStreamController
            .add(VideoEvent(eventType: VideoEventType.bufferingEnd));
        break;
      case 3: // MESessionStarted , occurs when user call play() or seekTo() in playing mode
        //log("[video_player_win] playback event: playing");
        value = value.copyWith(isInitialized: true, isPlaying: true, isCompleted: false);
        _startTrackingPosition(_lastSeekId);
        _eventStreamController
            .add(VideoEvent(eventType: VideoEventType.isPlayingStateUpdate));
        break;
      case 4: // MESessionPaused
        //log("[video_player_win] playback event: paused");
        value = value.copyWith(isPlaying: false);
        _eventStreamController
            .add(VideoEvent(eventType: VideoEventType.isPlayingStateUpdate));
        break;
      case 5: // MESessionStopped
        log("[video_player_win] playback event: stopped");
        value = value.copyWith(isPlaying: false);
        _eventStreamController
            .add(VideoEvent(eventType: VideoEventType.isPlayingStateUpdate));
        break;
      case 6: // MESessionEnded
        log("[video_player_win] playback event: play ended");
        _cancelTrackingPosition();
        value = value.copyWith(isPlaying: false, position: value.duration);
        if (_isLooping) {
          seekTo(Duration.zero);
        } else {
          value = value.copyWith(isCompleted: true);
          _eventStreamController
              .add(VideoEvent(eventType: VideoEventType.completed));
        }
        break;
      case 7: // MEError
        log("[video_player_win] playback event: error");
        value = value.copyWith(
            isInitialized: false, hasError: true, isPlaying: false);
        break;
    }
  }

  Future<void> initialize() async {
    WinVideoPlayerValue? pv = await VideoPlayerWinPlatform.instance
        .openVideo(this, textureId_, dataSource);
    if (pv == null) {
      log("[video_player_win] controller intialize (open video) failed");
      value = value.copyWith(hasError: true, isInitialized: false);
      _eventStreamController.add(VideoEvent(
          eventType: VideoEventType.initialized, duration: null, size: null));
      return;
    }
    textureId_ = pv.textureId;
    value = pv;

    _eventStreamController.add(VideoEvent(
      eventType: VideoEventType.initialized,
      duration: pv.duration,
      size: pv.size,
    ));
    log("flutter: video player file opened: id=$textureId_");
  }

  Future<void> play() async {
    if (!value.isInitialized) throw ArgumentError("video file not opened yet");
    await VideoPlayerWinPlatform.instance.play(textureId_);
  }

  Future<void> pause() async {
    if (!value.isInitialized) throw ArgumentError("video file not opened yet");
    await VideoPlayerWinPlatform.instance.pause(textureId_);
  }

  Timer? _seekTimer;
  Completer? completer;
  late Future<void> _lastSeekFuture;
  int _lastSeekPos = -1;
  Future<void> seekTo(Duration time) {
    if (!value.isInitialized) throw ArgumentError("video file not opened yet");
    value = value.copyWith(position: time, isCompleted: false);

    if (dataSourceType == WinDataSourceType.network) {
      // for network source, we delay 300ms for each seekTo() call, and cancel last seekTo() call if next seekTo() called in 300ms
      _lastSeekPos = time.inMilliseconds;
      if (completer == null) {
        _cancelTrackingPosition();
        completer = Completer();
        _lastSeekFuture = completer!.future;
      }

      _seekTimer?.cancel();
      _seekTimer = Timer(const Duration(milliseconds: 300), () async {
        _cancelTrackingPosition();
        await VideoPlayerWinPlatform.instance.seekTo(textureId_, _lastSeekPos);
        completer!.complete();
        completer = null;
      });
      return _lastSeekFuture;
    } else {
      // for non-network source, we call seekTo() every 300ms
      if (_seekTimer != null) {
        _lastSeekPos = time.inMilliseconds;
        return _lastSeekFuture;
      } else {
        completer = Completer();
        _lastSeekFuture = completer!.future;
        _seekTimer = Timer(const Duration(milliseconds: 300), () async {
          // avoid user call seekTo() many times in a short time
          _seekTimer = null;
          if (_lastSeekPos >= 0) {
            seekTo(Duration(milliseconds: _lastSeekPos)).then((_) {
              completer!.complete();
            });
            _lastSeekPos = -1;
          }
        });

        _cancelTrackingPosition();
        return VideoPlayerWinPlatform.instance
            .seekTo(textureId_, time.inMilliseconds);
      }
    }
  }

  Future<int> _getCurrentPosition() async {
    if (!value.isInitialized) throw ArgumentError("video file not opened yet");
    return await VideoPlayerWinPlatform.instance.getCurrentPosition(textureId_);
  }

  Future<void> setPlaybackSpeed(double speed) async {
    if (!value.isInitialized) throw ArgumentError("video file not opened yet");
    await VideoPlayerWinPlatform.instance.setPlaybackSpeed(textureId_, speed);
    value = value.copyWith(playbackSpeed: speed);
  }

  Future<void> setVolume(double volume) async {
    if (!value.isInitialized) throw ArgumentError("video file not opened yet");
    await VideoPlayerWinPlatform.instance.setVolume(textureId_, volume);
    value = value.copyWith(volume: volume);
  }

  Future<void> setLooping(bool looping) async {
    _isLooping = looping;
  }

  @override
  Future<void> dispose() async {
    VideoPlayerWinPlatform.instance.unregisterPlayer(textureId_);
    await VideoPlayerWinPlatform.instance.dispose(textureId_);
    _cancelTrackingPosition();

    textureId_ = -1;
    value.textureId = -1;
    value = value.copyWith(isInitialized: false, isPlaying: false);
    super.dispose();

    log("flutter: video player dispose: id=$textureId_");
  }
}

class WinVideoPlayer extends StatefulWidget {
  final WinVideoPlayerController controller;
  final FilterQuality filterQuality;

  // ignore: unused_element
  const WinVideoPlayer(this.controller,
      {Key? key, this.filterQuality = FilterQuality.low})
      : super(key: key);

  @override
  State<StatefulWidget> createState() => _WinVideoPlayerState();
}

class _WinVideoPlayerState extends State<WinVideoPlayer> {
  @override
  void didUpdateWidget(WinVideoPlayer oldWidget) {
    super.didUpdateWidget(oldWidget);

    if (widget.controller != oldWidget.controller) {
      setState(() {});
    }
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black,
      child: Center(
        child: AspectRatio(
          aspectRatio: widget.controller.value.aspectRatio,
          child: Texture(
            textureId: widget.controller.textureId_,
            filterQuality: widget.filterQuality,
          ),
        ),
      ),
    );
  }
}
