// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Audio format detection and client-side MP3 decoding. Lets the client accept the backend's
// default MP3 output (ElevenLabs mp3_44100_128) instead of requiring a WAV wrapper on the PHP
// side: everything is normalized to a 16-bit PCM WAV buffer, the only format the rest of the
// pipeline (FWaveModInfo, USoundWaveProcedural, Rhubarb) understands.
namespace AudioFormatUtils
{
	// "RIFF....WAVE" at the start of the buffer.
	bool IsWav(const TArray<uint8>& Bytes);

	// ID3 tag or raw MPEG frame sync (0xFF 0xEx) at the start of the buffer.
	bool IsMp3(const TArray<uint8>& Bytes);

	// Decodes MP3 to 16-bit PCM (dr_mp3) and wraps it in a WAV header.
	// Returns false (and logs) if decoding fails.
	bool DecodeMp3ToWav(const TArray<uint8>& Mp3Bytes, TArray<uint8>& OutWavBytes);
}
