# RealTimeLipSync: Unreal Client

Unreal Engine 5.5 client that drives MetaHuman facial animation (lipsync) from
audio, using [Rhubarb Lip Sync](https://github.com/DanielSWolf/rhubarb-lip-sync)
(external CLI, MIT license) rather than a paid plugin.

> **Scope**: this guide covers opening and running the Unreal project **from
> source, in the editor**. It complements `LAMP-SETUP-README.md`, which covers
> the PHP/Slim middleware. Together they let a reviewer reproduce the full
> demo: middleware running on a LAMP server, Unreal client opened in the
> editor and pointed at that server (see section 4, `BackendBaseUrl`). No
> packaged build of this project exists yet (see section 7, known limitations).

## 1. Prerequisites

1. **Unreal Engine 5.5**
2. **Visual Studio 2022** (compiler toolchain for UE C++)
3. **Rhubarb Lip Sync** binary: download a release from
   [DanielSWolf/rhubarb-lip-sync](https://github.com/DanielSWolf/rhubarb-lip-sync/releases)
   and note the path to `rhubarb.exe` (default expected by this project:
   `C:/Tools/Rhubarb-Lip-Sync-1.14.0-Windows/rhubarb.exe`, but every actor exposes
   its own `RhubarbExecutablePath` property, see section 4).
4. **MetaHuman assets** (Ada/Taro or your own) imported via Fab, needed by the
   actors that animate a MetaHuman face. Not required to test the pipeline on
   the simple test mesh (`ARhubarbTestActor`, section 4).
5. Enabled plugins (already set in `RealTimeLipSync.uproject`): `AppleARKitFaceSupport`,
   `LiveLinkControlRig`, `ModelingToolsEditorMode`.
6. For Phase 2 (dynamic/backend audio): a running instance of the PHP/Slim
   middleware. See the separate backend repo's `LAMP-SETUP-README.md` for how to
   stand one up, or `php -S localhost:8080 -t backend/public` for a quick local run.

## 2. Getting the project running

```
Right click RealTimeLipSync.uproject → Generate Visual Studio project files
Open RealTimeLipSync.sln → build (Development Editor) → launch from VS, or
double click the .uproject once built.
```

A **full rebuild** is required after pulling changes
that add or rename `UPROPERTY`/`UFUNCTION` members

## 3. Source layout

```
Source/RealTimeLipSync/
  Rhubarb/      URhubarbLipSyncRunner: launches rhubarb.exe as an external
                process (FPlatformProcess::CreateProc), parses the resulting
                JSON mouth cue timeline (Json/JsonUtilities modules).
  FaceDriver/   FRhubarbLiveLinkSource (custom ILiveLinkSource pushing ARKit
                curves), VisemeToArKitMapping (viseme A to X → ARKit blendshape
                weights table), FIdleFaceAnimator (blink / idle micro movement).
  Http/         FMiddlewareAuthClient: reproduces the backend's HMAC guard
                flow (session id/secret, request signing) to call
                /api/v1/ai/tts and /api/v1/ai/ask.
  Test/         Actors: RhubarbTestActor, RhubarbFaceActorBase (shared base),
                RhubarbMetaHumanActor, DynamicSpeechTestActor, DemoScenarioActor.
```

`RealTimeLipSync.Build.cs` declares each of the above subfolders in
`PrivateIncludePaths`. UBT does not add module subfolders to the include path
by default, so bare `#include "SomeHeader.h"` across folders depends on that.

## 4. Actors and test maps

### TestMap: ARhubarbTestActor

Simple 6 to 9 blendshape test mesh (Blender shape keys `Viseme_A`…`Viseme_X`).
Validates the Rhubarb → JSON → morph target pipeline without MetaHuman complexity.

### TestMetaHuman: ARhubarbMetaHumanActor

Phase 1: plays a pre recorded, **imported** `USoundWave` (`SoundToPlay`), runs
Rhubarb blocking (local file), drives a MetaHuman via LiveLink/ARKit curves.
Has a debug mode (`bDebugMode`) with 14 manual weight sliders to hand calibrate
`VisemeToArKitMapping`.

### TestStreaming / TestStreamingMap: ADynamicSpeechTestActor

Phase 2a: either simulates a backend chunk from a local `.wav`
(`SimulateIncomingChunk`) or calls the real backend (`RequestSpeechFromBackend`,
signed `GET /api/v1/ai/tts`). Runs Rhubarb async off the game thread.

### DemoScenario / TestDemoScenario: ADemoScenarioActor

"Near finished product" demo: `PlayIntro()` plays a pre generated, imported
intro line (no network); `AskQuestion()` sends free text to `/api/v1/ai/ask`
(ChatGPT) then speaks the reply through the full TTS/Rhubarb pipeline.

`ARhubarbMetaHumanActor`, `ADynamicSpeechTestActor`, and `ADemoScenarioActor`
all derive from **`ARhubarbFaceActorBase`**, which owns the shared LiveLink
setup, viseme sampling/smoothing (`VisemeInterpSpeed`), blink (`FIdleFaceAnimator`),
lip sync delay compensation (`LipSyncDelaySeconds`), and the
"WAV bytes → temp file → async Rhubarb → `Play()`" pipeline
(`ProcessIncomingAudioChunk`) with latency tracing
(`Saved/DynamicSpeech/latency_log.csv`).

### Per actor setup checklist

1. **`RhubarbExecutablePath`** (every actor that runs Rhubarb): point it at
   your local `rhubarb.exe`.
2. **MetaHuman actors** (`RhubarbMetaHumanActor`/`DynamicSpeechTestActor`/`DemoScenarioActor`):
   on the MetaHuman Blueprint (`BP_Ada`/`BP_Taro`) enable **"Use ARKit Face"**
   and set **"ARKit Face Subject"** to match the actor's `LiveLinkSubjectName`
   (default `"RhubarbLipSync"` everywhere, so you generally don't need to touch it
   unless two such actors are alive in the same map at once). Assign the
   MetaHuman actor to `BodyActor` on the C++ actor if you want the idle body
   animation (`IdleBodyAnimation`, retargeted Mixamo) to play.
3. **`DynamicSpeechTestActor` / `DemoScenarioActor`**: set `BackendBaseUrl`
   (default `http://localhost:8080`, scheme+host only, no `/api/v1`) to wherever
   your backend instance runs.
4. **`RhubarbMetaHumanActor` / `DemoScenarioActor`**: assign an imported
   `USoundWave` to `SoundToPlay`. Its source `.wav` path is resolved from
   `AssetImportData`, which is **editor only** and will not work in a packaged
   build (see section 7).

## 5. Running a quick smoke test

1. Open `TestMap`, place/select `BP_RhubarbTest` (or the C++ actor directly),
   assign a `.wav`, press Play. Mouth cues should print to the log and morph
   targets should animate.
2. Open `TestMetaHuman`, assign `SoundToPlay`, verify "Use ARKit Face" is set
   on the MetaHuman, press Play. The MetaHuman's mouth should move in sync.
3. For the backend path: start the PHP backend locally, open `TestStreaming`,
   set `BackendBaseUrl`, click `RequestSpeechFromBackend` in the Details panel
   (editor button) while in Play. Check `Saved/DynamicSpeech/latency_log.csv`
   for the recorded pipeline timings.

## 6. Audio format expected from the backend

The client currently only accepts **WAV** (RIFF container, 16 bit PCM), never
MP3. `ProcessIncomingAudioChunk` parses the response body with
`FWaveModInfo::ReadWaveInfo` and feeds the PCM to
`USoundWaveProcedural::QueueAudio`; both need a real WAV header, not raw PCM
and not MP3.

ElevenLabs itself never returns a WAV container (its `pcm_*` output formats
are headerless raw PCM, and its default output is MP3), so any backend
endpoint that hands audio to this client must wrap the raw PCM in a WAV header
before responding. The existing `/api/v1/ai/tts?fmt=wav` endpoint
(`backend/public/index.php`) already does this: it requests
`output_format=pcm_16000` from ElevenLabs, then calls this function to add the
header before returning the response:

```php
function wrap_pcm_as_wav(string $pcm, int $sampleRate, int $channels = 1, int $bitsPerSample = 16): string {
    $byteRate   = $sampleRate * $channels * ($bitsPerSample / 8);
    $blockAlign = $channels * ($bitsPerSample / 8);

    // ElevenLabs' PCM stream can end on a size not aligned to blockAlign; truncate
    // to the nearest full sample or QueueAudio's "BufferSize % SampleByteSize == 0"
    // ensure fires on the Unreal side.
    $alignedLen = strlen($pcm) - (strlen($pcm) % $blockAlign);
    if ($alignedLen !== strlen($pcm)) {
        $pcm = substr($pcm, 0, $alignedLen);
    }
    $dataLen = strlen($pcm);

    $header  = 'RIFF';
    $header .= pack('V', 36 + $dataLen);
    $header .= 'WAVE';
    $header .= 'fmt ';
    $header .= pack('V', 16);
    $header .= pack('v', 1);
    $header .= pack('v', $channels);
    $header .= pack('V', $sampleRate);
    $header .= pack('V', (int)$byteRate);
    $header .= pack('v', (int)$blockAlign);
    $header .= pack('v', $bitsPerSample);
    $header .= 'data';
    $header .= pack('V', $dataLen);

    return $header . $pcm;
}
```

Any future endpoint built for Phase 2b (`speak/start`/`speak/chunk`, see
`TODO.md`) needs to reuse this same function on its audio response, otherwise
the Unreal client will not be able to parse it.

## 7. Known limitations

1. `ARhubarbTestActor::SoundToPlay` and `ARhubarbMetaHumanActor`/`ADemoScenarioActor`'s
   `SoundToPlay` resolve their disk path via `USoundWave::AssetImportData`
   (`WITH_EDITORONLY_DATA`). This breaks once the game is packaged. A manual
   path field or bundling the source `.wav` outside `Content/` will be needed
   before any packaged build.
2. Rhubarb processes whole audio files, not a live stream; Phase 2's chunk by
   chunk approach (`speak/start`/`speak/chunk`, not yet implemented on the
   backend) works around this by running Rhubarb once per short chunk rather
   than on a continuous stream.
3. `FPlatformMisc::GetSHA256Signature` is not implemented on Windows in this
   engine version, so `MiddlewareAuthClient` implements SHA-256/HMAC by hand
   rather than using an engine API.