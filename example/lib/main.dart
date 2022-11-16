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

  late WinVideoPlayerController controller;

  @override
  void initState() {
    super.initState();
    controller = WinVideoPlayerController.file(File("E:\\test_youtube.mp4"));
    controller.initialize().then((value) {
      if (controller.value.isInitialized) {
        controller.play();
        setState(() {});
      } else {
        log("video file load failed");
      }
    }).catchError((e) {
      log("controller.initialize() error occurs: $e");
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
          WinVideoPlayer(controller),
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
