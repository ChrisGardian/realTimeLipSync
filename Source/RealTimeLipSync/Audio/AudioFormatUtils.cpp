// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioFormatUtils.h"

THIRD_PARTY_INCLUDES_START
#include "dr_mp3.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	void AppendUInt32(TArray<uint8>& Out, uint32 Value)
	{
		// WAV is little-endian regardless of the host platform.
		Out.Add(Value & 0xFF);
		Out.Add((Value >> 8) & 0xFF);
		Out.Add((Value >> 16) & 0xFF);
		Out.Add((Value >> 24) & 0xFF);
	}

	void AppendUInt16(TArray<uint8>& Out, uint16 Value)
	{
		Out.Add(Value & 0xFF);
		Out.Add((Value >> 8) & 0xFF);
	}

	void AppendTag(TArray<uint8>& Out, const char* Tag)
	{
		Out.Append(reinterpret_cast<const uint8*>(Tag), 4);
	}

	// 44-byte canonical PCM WAV header, same layout as wrap_pcm_as_wav() in the backend's index.php.
	void BuildWavHeader(TArray<uint8>& Out, uint32 SampleRate, uint16 NumChannels, uint32 DataSize)
	{
		const uint16 BitsPerSample = 16;
		const uint16 BlockAlign = NumChannels * (BitsPerSample / 8);
		const uint32 ByteRate = SampleRate * BlockAlign;

		AppendTag(Out, "RIFF");
		AppendUInt32(Out, 36 + DataSize);
		AppendTag(Out, "WAVE");

		AppendTag(Out, "fmt ");
		AppendUInt32(Out, 16);  // fmt chunk size
		AppendUInt16(Out, 1);   // PCM
		AppendUInt16(Out, NumChannels);
		AppendUInt32(Out, SampleRate);
		AppendUInt32(Out, ByteRate);
		AppendUInt16(Out, BlockAlign);
		AppendUInt16(Out, BitsPerSample);

		AppendTag(Out, "data");
		AppendUInt32(Out, DataSize);
	}
}

bool AudioFormatUtils::IsWav(const TArray<uint8>& Bytes)
{
	return Bytes.Num() >= 12
		&& FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) == 0
		&& FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) == 0;
}

bool AudioFormatUtils::IsMp3(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() < 3)
	{
		return false;
	}
	const bool bHasId3Tag = FMemory::Memcmp(Bytes.GetData(), "ID3", 3) == 0;
	const bool bHasFrameSync = Bytes[0] == 0xFF && (Bytes[1] & 0xE0) == 0xE0;
	return bHasId3Tag || bHasFrameSync;
}

bool AudioFormatUtils::DecodeMp3ToWav(const TArray<uint8>& Mp3Bytes, TArray<uint8>& OutWavBytes)
{
	drmp3_config Config;
	drmp3_uint64 FrameCount = 0;
	drmp3_int16* Pcm = drmp3_open_memory_and_read_pcm_frames_s16(Mp3Bytes.GetData(), Mp3Bytes.Num(), &Config, &FrameCount, nullptr);
	if (!Pcm || FrameCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("AudioFormatUtils: MP3 decoding failed (%d input bytes)"), Mp3Bytes.Num());
		drmp3_free(Pcm, nullptr);
		return false;
	}

	const uint64 PcmSize = FrameCount * Config.channels * sizeof(drmp3_int16);
	if (PcmSize > static_cast<uint64>(MAX_int32 - 44))
	{
		UE_LOG(LogTemp, Error, TEXT("AudioFormatUtils: decoded MP3 too large for a WAV buffer (%llu bytes)"), PcmSize);
		drmp3_free(Pcm, nullptr);
		return false;
	}

	// Copy into UE-owned memory and release dr_mp3's buffer right away, so callers only deal with TArray.
	OutWavBytes.Reset(44 + static_cast<int32>(PcmSize));
	BuildWavHeader(OutWavBytes, Config.sampleRate, static_cast<uint16>(Config.channels), static_cast<uint32>(PcmSize));
	OutWavBytes.Append(reinterpret_cast<const uint8*>(Pcm), static_cast<int32>(PcmSize));
	drmp3_free(Pcm, nullptr);

	UE_LOG(LogTemp, Log, TEXT("AudioFormatUtils: MP3 decoded (%u Hz, %u ch, %llu frames)"), Config.sampleRate, Config.channels, FrameCount);
	return true;
}
