// Copyright Epic Games, Inc. All Rights Reserved.

#include "MiddlewareAuthClient.h"

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// --- SHA-256 minimal, autoportant (algorithme domaine public, adapté de l'implémentation de
	// référence de Brad Conte). Nécessaire car FPlatformMisc::GetSHA256Signature n'est PAS
	// implémenté sur Windows : la version générique (GenericPlatformMisc.cpp) fait juste
	// "checkf(false, ...)" et aucune override Windows n'existe dans les sources du moteur (vérifié).
	// HMAC-SHA256 est construit par-dessus, comme le hash_hmac('sha256', ...) utilisé côté PHP.

	struct FSha256Context
	{
		uint8 Data[64] = {};
		uint32 DataLen = 0;
		uint64 BitLen = 0;
		uint32 State[8] = {};
	};

	constexpr uint32 GSha256K[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
		0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
		0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
		0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
		0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
		0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
	};

	FORCEINLINE uint32 RotR(uint32 X, uint32 N)
	{
		return (X >> N) | (X << (32 - N));
	}

	void Sha256Transform(FSha256Context& Ctx, const uint8* Block)
	{
		uint32 W[64];
		for (int32 I = 0; I < 16; ++I)
		{
			W[I] = (static_cast<uint32>(Block[I * 4]) << 24) | (static_cast<uint32>(Block[I * 4 + 1]) << 16)
				| (static_cast<uint32>(Block[I * 4 + 2]) << 8) | static_cast<uint32>(Block[I * 4 + 3]);
		}
		for (int32 I = 16; I < 64; ++I)
		{
			const uint32 S0 = RotR(W[I - 15], 7) ^ RotR(W[I - 15], 18) ^ (W[I - 15] >> 3);
			const uint32 S1 = RotR(W[I - 2], 17) ^ RotR(W[I - 2], 19) ^ (W[I - 2] >> 10);
			W[I] = W[I - 16] + S0 + W[I - 7] + S1;
		}

		uint32 A = Ctx.State[0], B = Ctx.State[1], C = Ctx.State[2], D = Ctx.State[3];
		uint32 E = Ctx.State[4], F = Ctx.State[5], G = Ctx.State[6], H = Ctx.State[7];

		for (int32 I = 0; I < 64; ++I)
		{
			const uint32 S1 = RotR(E, 6) ^ RotR(E, 11) ^ RotR(E, 25);
			const uint32 Ch = (E & F) ^ (~E & G);
			const uint32 Temp1 = H + S1 + Ch + GSha256K[I] + W[I];
			const uint32 S0 = RotR(A, 2) ^ RotR(A, 13) ^ RotR(A, 22);
			const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
			const uint32 Temp2 = S0 + Maj;

			H = G; G = F; F = E; E = D + Temp1;
			D = C; C = B; B = A; A = Temp1 + Temp2;
		}

		Ctx.State[0] += A; Ctx.State[1] += B; Ctx.State[2] += C; Ctx.State[3] += D;
		Ctx.State[4] += E; Ctx.State[5] += F; Ctx.State[6] += G; Ctx.State[7] += H;
	}

	void Sha256Init(FSha256Context& Ctx)
	{
		Ctx.DataLen = 0;
		Ctx.BitLen = 0;
		Ctx.State[0] = 0x6a09e667; Ctx.State[1] = 0xbb67ae85; Ctx.State[2] = 0x3c6ef372; Ctx.State[3] = 0xa54ff53a;
		Ctx.State[4] = 0x510e527f; Ctx.State[5] = 0x9b05688c; Ctx.State[6] = 0x1f83d9ab; Ctx.State[7] = 0x5be0cd19;
	}

	void Sha256Update(FSha256Context& Ctx, const uint8* Data, int64 Len)
	{
		for (int64 I = 0; I < Len; ++I)
		{
			Ctx.Data[Ctx.DataLen++] = Data[I];
			if (Ctx.DataLen == 64)
			{
				Sha256Transform(Ctx, Ctx.Data);
				Ctx.BitLen += 512;
				Ctx.DataLen = 0;
			}
		}
	}

	void Sha256Final(FSha256Context& Ctx, uint8 OutHash[32])
	{
		uint32 I = Ctx.DataLen;

		if (Ctx.DataLen < 56)
		{
			Ctx.Data[I++] = 0x80;
			while (I < 56) Ctx.Data[I++] = 0x00;
		}
		else
		{
			Ctx.Data[I++] = 0x80;
			while (I < 64) Ctx.Data[I++] = 0x00;
			Sha256Transform(Ctx, Ctx.Data);
			FMemory::Memzero(Ctx.Data, 56);
		}

		Ctx.BitLen += static_cast<uint64>(Ctx.DataLen) * 8;
		for (int32 B = 0; B < 8; ++B)
		{
			Ctx.Data[63 - B] = static_cast<uint8>(Ctx.BitLen >> (B * 8));
		}
		Sha256Transform(Ctx, Ctx.Data);

		for (I = 0; I < 4; ++I)
		{
			for (int32 J = 0; J < 8; ++J)
			{
				OutHash[J * 4 + I] = static_cast<uint8>((Ctx.State[J] >> (24 - I * 8)) & 0xff);
			}
		}
	}

	void Sha256(const uint8* Data, int64 Length, uint8 OutHash[32])
	{
		FSha256Context Ctx;
		Sha256Init(Ctx);
		Sha256Update(Ctx, Data, Length);
		Sha256Final(Ctx, OutHash);
	}

	TArray<uint8> ComputeHmacSha256(const TArray<uint8>& KeyBytesIn, const TArray<uint8>& MessageBytes)
	{
		constexpr int32 BlockSize = 64; // taille de bloc SHA-256

		TArray<uint8> Key = KeyBytesIn;
		if (Key.Num() > BlockSize)
		{
			uint8 Hashed[32];
			Sha256(Key.GetData(), Key.Num(), Hashed);
			Key = TArray<uint8>(Hashed, 32);
		}
		Key.SetNumZeroed(BlockSize);

		TArray<uint8> InnerPad, OuterPad;
		InnerPad.SetNumUninitialized(BlockSize);
		OuterPad.SetNumUninitialized(BlockSize);
		for (int32 Index = 0; Index < BlockSize; ++Index)
		{
			InnerPad[Index] = Key[Index] ^ 0x36;
			OuterPad[Index] = Key[Index] ^ 0x5c;
		}

		TArray<uint8> InnerData = InnerPad;
		InnerData.Append(MessageBytes);
		uint8 InnerHash[32];
		Sha256(InnerData.GetData(), InnerData.Num(), InnerHash);

		TArray<uint8> OuterData = OuterPad;
		OuterData.Append(InnerHash, 32);
		uint8 FinalHash[32];
		Sha256(OuterData.GetData(), OuterData.Num(), FinalHash);

		return TArray<uint8>(FinalHash, 32);
	}

	FString HexEncode(const TArray<uint8>& Bytes)
	{
		FString Out;
		Out.Reserve(Bytes.Num() * 2);
		for (uint8 Byte : Bytes)
		{
			Out += FString::Printf(TEXT("%02x"), Byte);
		}
		return Out;
	}

	TArray<uint8> HexDecode(const FString& Hex)
	{
		TArray<uint8> Out;
		Out.Reserve(Hex.Len() / 2);
		for (int32 Index = 0; Index + 1 < Hex.Len(); Index += 2)
		{
			Out.Add(static_cast<uint8>(FParse::HexNumber(*Hex.Mid(Index, 2))));
		}
		return Out;
	}
}

void FMiddlewareAuthClient::RequestSession(const FString& BackendBaseUrl, TFunction<void(bool, FString, FString)> OnComplete)
{
	const TSharedRef<IHttpRequest> IdRequest = FHttpModule::Get().CreateRequest();
	IdRequest->SetURL(BackendBaseUrl + TEXT("/api/v1/session/id"));
	IdRequest->SetVerb(TEXT("GET"));
	IdRequest->OnProcessRequestComplete().BindLambda(
		[BackendBaseUrl, OnComplete](FHttpRequestPtr, const FHttpResponsePtr& IdResponse, bool bIdSuccess)
	{
		if (!bIdSuccess || !IdResponse.IsValid() || IdResponse->GetResponseCode() != 200)
		{
			UE_LOG(LogTemp, Error, TEXT("MiddlewareAuthClient: /session/id failed (code %d)"),
				IdResponse.IsValid() ? IdResponse->GetResponseCode() : -1);
			OnComplete(false, FString(), FString());
			return;
		}

		FString Sid;
		TSharedPtr<FJsonObject> IdJson;
		const TSharedRef<TJsonReader<>> IdReader = TJsonReaderFactory<>::Create(IdResponse->GetContentAsString());
		if (!FJsonSerializer::Deserialize(IdReader, IdJson) || !IdJson.IsValid() || !IdJson->TryGetStringField(TEXT("sid"), Sid))
		{
			UE_LOG(LogTemp, Error, TEXT("MiddlewareAuthClient: /session/id returned unexpected body: %s"), *IdResponse->GetContentAsString());
			OnComplete(false, FString(), FString());
			return;
		}

		const TSharedRef<IHttpRequest> SecretRequest = FHttpModule::Get().CreateRequest();
		SecretRequest->SetURL(BackendBaseUrl + TEXT("/api/v1/session/secret?sid=") + FGenericPlatformHttp::UrlEncode(Sid));
		SecretRequest->SetVerb(TEXT("GET"));
		SecretRequest->OnProcessRequestComplete().BindLambda(
			[Sid, OnComplete](FHttpRequestPtr, const FHttpResponsePtr& SecretResponse, bool bSecretSuccess)
		{
			if (!bSecretSuccess || !SecretResponse.IsValid() || SecretResponse->GetResponseCode() != 200)
			{
				UE_LOG(LogTemp, Error, TEXT("MiddlewareAuthClient: /session/secret failed (code %d)"),
					SecretResponse.IsValid() ? SecretResponse->GetResponseCode() : -1);
				OnComplete(false, FString(), FString());
				return;
			}

			FString Secret;
			TSharedPtr<FJsonObject> SecretJson;
			const TSharedRef<TJsonReader<>> SecretReader = TJsonReaderFactory<>::Create(SecretResponse->GetContentAsString());
			if (!FJsonSerializer::Deserialize(SecretReader, SecretJson) || !SecretJson.IsValid() || !SecretJson->TryGetStringField(TEXT("secret"), Secret))
			{
				UE_LOG(LogTemp, Error, TEXT("MiddlewareAuthClient: /session/secret returned unexpected body: %s"), *SecretResponse->GetContentAsString());
				OnComplete(false, FString(), FString());
				return;
			}

			OnComplete(true, Sid, Secret);
		});
		SecretRequest->ProcessRequest();
	});
	IdRequest->ProcessRequest();
}

FString FMiddlewareAuthClient::BuildSignedUrl(const FString& BackendBaseUrl, const FString& Path,
	TMap<FString, FString> QueryParams, const FString& Sid, const FString& SecretHex)
{
	// SIGN_TTL_SEC=60 côté backend (.env) : la signature doit rester dans cette fenêtre.
	const int64 Exp = FDateTime::UtcNow().ToUnixTimestamp() + 60;
	const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::Digits);

	QueryParams.Add(TEXT("sid"), Sid);
	QueryParams.Add(TEXT("exp"), FString::Printf(TEXT("%lld"), Exp));
	QueryParams.Add(TEXT("nonce"), Nonce);
	QueryParams.KeySort([](const FString& A, const FString& B) { return A < B; });

	// Équivalent de canon_query() côté PHP : tri alphabétique des clés, encodage RFC3986, sig exclu.
	FString CanonicalQuery;
	for (const TPair<FString, FString>& Pair : QueryParams)
	{
		if (!CanonicalQuery.IsEmpty())
		{
			CanonicalQuery += TEXT("&");
		}
		CanonicalQuery += FGenericPlatformHttp::UrlEncode(Pair.Key) + TEXT("=") + FGenericPlatformHttp::UrlEncode(Pair.Value);
	}

	// Équivalent de string_to_sign() côté PHP : "METHODE\nPATH\nquery_canonique".
	const FString StringToSign = FString::Printf(TEXT("GET\n%s\n%s"), *Path, *CanonicalQuery);

	const FTCHARToUTF8 MessageUtf8(*StringToSign);
	const TArray<uint8> MessageBytes(reinterpret_cast<const uint8*>(MessageUtf8.Get()), MessageUtf8.Length());
	const TArray<uint8> KeyBytes = HexDecode(SecretHex);

	const FString Signature = HexEncode(ComputeHmacSha256(KeyBytes, MessageBytes));

	return BackendBaseUrl + Path + TEXT("?") + CanonicalQuery + TEXT("&sig=") + Signature;
}
