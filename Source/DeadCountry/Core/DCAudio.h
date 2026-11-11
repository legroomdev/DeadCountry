#pragma once

#include "CoreMinimal.h"

/** Every sound the game plays, by gameplay meaning (not by file). */
enum class EDCSound : uint8
{
	Gunshot,
	DryFire,
	MagOut,
	MagIn,
	SlideRelease,
	CasingTink,
	Footstep,
	ImpactDirt,
	ImpactFlesh,
	ZombieGroan,
	ZombieAlert,
	ZombieAttack,
	MeleeSwing,
	MeleeHitBlunt,
	MeleeHitBlade,
	Shove,
	WeaponBreak,
	MAX
};

/**
 * Tiny sound library: picks a random variant (/Game/Audio/SW_<Name>_NN), applies pitch jitter and a
 * distance attenuation that suits the sound (gunshots carry ~120m, footsteps ~15m).
 * Swap the WAVs in SourceArt/Audio and re-import to change any sound - no code changes needed.
 */
namespace DCAudio
{
	DEADCOUNTRY_API void PlayAt(const UObject* WorldContext, EDCSound Sound, const FVector& Location, float Volume = 1.f, float PitchScale = 1.f);
}
