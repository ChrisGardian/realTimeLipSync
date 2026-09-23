# RealTimeLipSync

Unreal Engine 5.5 client for the Meta Serious Game middleware: a MetaHuman
patient whose face is animated in real time (lip sync, blinking) from the audio
returned by the backend. The player asks free-form questions and has to find
out which disorder the patient suffers from.

Pipeline: question → `/api/v1/ai/ask` (ChatGPT) → `/api/v1/ai/tts` (ElevenLabs,
MP3) → MP3 decoded on the client → [Rhubarb Lip Sync](https://github.com/DanielSWolf/rhubarb-lip-sync)
(mouth shapes) → ARKit curves pushed to the MetaHuman via LiveLink.

The backend is used **unmodified**: the client only needs a running instance
of the PHP/Slim middleware.

## 1. Run the demo (packaged build)

No Unreal Engine or Visual Studio needed.

1. Unzip the build folder anywhere. If Windows complains about missing runtime
   libraries, run `Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe` once.
2. Start it, pointing it at your backend:
   ```
   RealTimeLipSync.exe -BackendUrl=https://your-server.example
   ```
   Tip: create a shortcut to `RealTimeLipSync.exe` and append the argument in
   *Properties → Target*. Add `-log` to open a live log console.
   Without the argument, `http://localhost:8080` is used.
   The game runs fullscreen; add `-windowed -ResX=1600 -ResY=900` for a window.
3. Click **Szenario starten**: the intro plays, then type questions in the
   bar at the bottom (Enter or **Fragen**). **Beenden** (top right) quits.

The backend must be served at the **root** of its (sub)domain
(`DocumentRoot` on `backend/public`): request signing covers the full path.

Logs: `RealTimeLipSync/Saved/Logs/RealTimeLipSync.log`.
Pipeline timings: `RealTimeLipSync/Saved/DynamicSpeech/latency_log.csv`.

## 2. Open the project from source

Requirements:
- **Git LFS**, installed *before* cloning (all assets are stored in LFS).
- **Unreal Engine 5.5** (Epic Games Launcher).
- **Visual Studio** with the "Game development with C++" workload, only
  needed to compile the C++ code. The `.vsconfig` file at the root lists the
  required components (Visual Studio Installer → *More → Import configuration*).

Rhubarb is included in `ThirdParty/Rhubarb/`, nothing else to download.

Double click `RealTimeLipSync.uproject` (accept rebuilding the module if
asked). The editor opens on the `DemoScenario` map; press Play.

To package: *Platforms → Windows → Package Project* (settings are already
configured: Development, `DemoScenario` map only).

## 3. Settings

Everything is on the `DemoScenarioActor` in the `DemoScenario` map
(Details panel, category *RhubarbLipSync*):

| Property | Purpose |
|---|---|
| `BackendBaseUrl` | Middleware address (overridden by `-BackendUrl=`) |
| `QuestionContext` | Role prompt sent with every question (patient, disorder to guess) |
| `IntroAudioPath` | Intro audio, relative to `Content/` (WAV or MP3) |
| `ScenarioTitle`, `ScenarioDescription` | Start screen texts |
| `IntroDelaySeconds` | Pause between the start click and the intro |
| `bShowDemoUi` | Turn off the start screen/question bar (editor testing) |
| `LipSyncDelaySeconds`, `VisemeInterpSpeed` | Lip sync timing and smoothing |

Files outside assets that ship with the build: `Content/NonAssets/` (intro
audio) and `ThirdParty/Rhubarb/`.

### Tuning individual mouth shapes

Rhubarb outputs 9 mouth shapes (A–H, X; see the
[Rhubarb docs](https://github.com/DanielSWolf/rhubarb-lip-sync#mouth-shapes)).
Each one is converted to ARKit blendshape weights in
`VisemeToArKitMapping::GetWeightsForViseme`
(`Source/RealTimeLipSync/FaceDriver/VisemeToArKitMapping.cpp`), one block per
shape. If a shape looks wrong (e.g. lips not closed enough on M/B/P = shape
`A`), adjust its weights there and recompile.

To find good values without recompiling, use the `TestMetaHuman` map: select
the `RhubarbMetaHumanActor`, enable `bDebugMode` (category *RhubarbLipSync →
Debug*) and press Play. Then either:
- keep `bDebugUseManualWeights` on and move the `DebugWeight_*` sliders
  (0–1, applied live to the face), or
- turn it off and set `DebugForcedViseme` (A–H, X) to preview what the current
  table produces for that shape.

Copy the values you like into `GetWeightsForViseme`.

## 4. Source layout

```
Source/RealTimeLipSync/
  Rhubarb/     runs rhubarb.exe and parses its mouth cue JSON
  Audio/       WAV/MP3 detection, MP3 decoding (dr_mp3)
  FaceDriver/  LiveLink source, mouth shape → ARKit weights, blinking
  Http/        backend session + request signing (HMAC)
  UI/          start screen and question bar (Slate)
  Test/        DemoScenarioActor (the demo) and the earlier test actors
```

The other maps (`TestMap`, `TestMetaHuman`, `TestStreaming`) are development
test benches from the thesis and are not part of the demo.

## 5. Third party

- [Rhubarb Lip Sync](https://github.com/DanielSWolf/rhubarb-lip-sync) 1.14.0,
  MIT (dependencies under BSD/Boost licenses), see `ThirdParty/Rhubarb/LICENSE.md`.
- [dr_mp3](https://github.com/mackron/dr_libs) 0.7.3, public domain / MIT-0,
  see the end of `Source/RealTimeLipSync/Audio/dr_mp3.h`.
