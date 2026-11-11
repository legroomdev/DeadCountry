#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDC, Log, All);

// Custom trace channel defined in Config/DefaultEngine.ini ("Weapon"). Blocks by default,
// so pawn capsules and world geometry stop bullets without per-actor setup.
#define DC_TRACE_WEAPON ECC_GameTraceChannel1
