// Compiles the dr_mp3 decoder (single-header C library, public domain / MIT-0, v0.7.3,
// github.com/mackron/dr_libs, commit 5690d46). This must stay the only file defining
// DR_MP3_IMPLEMENTATION; everywhere else just includes dr_mp3.h for the declarations.

#include "CoreMinimal.h"

// Silences the third-party warnings UE treats as errors (implicit conversions, etc.).
THIRD_PARTY_INCLUDES_START
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#undef DR_MP3_IMPLEMENTATION
THIRD_PARTY_INCLUDES_END
