#include "Core/DCAudio.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"

namespace
{
	struct FSoundSpec
	{
		const TCHAR* BaseName;   // asset name without _NN
		int32 Variants;          // 0 = single asset without a suffix
		float FalloffCm;         // distance at which it fades to silence
		float PitchJitter;       // +/- random pitch
	};

	const FSoundSpec GSpecs[] = {
		{ TEXT("SW_Gunshot_P9"),   3, 12000.f, 0.04f }, // Gunshot
		{ TEXT("SW_DryFire"),      0,   800.f, 0.05f }, // DryFire
		{ TEXT("SW_MagOut"),       0,  1000.f, 0.04f }, // MagOut
		{ TEXT("SW_MagIn"),        0,  1000.f, 0.04f }, // MagIn
		{ TEXT("SW_SlideRelease"), 0,  1500.f, 0.04f }, // SlideRelease
		{ TEXT("SW_CasingTink"),   3,   900.f, 0.10f }, // CasingTink
		{ TEXT("SW_Footstep_Dirt"),4,  1500.f, 0.08f }, // Footstep
		{ TEXT("SW_Impact_Dirt"),  3,  2500.f, 0.08f }, // ImpactDirt
		{ TEXT("SW_Impact_Flesh"), 2,  2000.f, 0.08f }, // ImpactFlesh
		{ TEXT("SW_Zombie_Groan"), 4,  2500.f, 0.10f }, // ZombieGroan
		{ TEXT("SW_Zombie_Alert"), 2,  4000.f, 0.08f }, // ZombieAlert
		{ TEXT("SW_Zombie_Attack"),2,  2000.f, 0.08f }, // ZombieAttack
		{ TEXT("SW_Melee_Swing"),  3,   900.f, 0.08f }, // MeleeSwing
		{ TEXT("SW_Melee_HitBlunt"),3, 1800.f, 0.08f }, // MeleeHitBlunt
		{ TEXT("SW_Melee_HitBlade"),2, 1600.f, 0.08f }, // MeleeHitBlade
		{ TEXT("SW_Shove"),        0,  1200.f, 0.10f }, // Shove
		{ TEXT("SW_WeaponBreak"),  0,  2000.f, 0.05f }, // WeaponBreak
	};
	static_assert(UE_ARRAY_COUNT(GSpecs) == static_cast<int32>(EDCSound::MAX), "One spec per EDCSound");

	struct FLoadedSound
	{
		TArray<USoundBase*> Variants;
		USoundAttenuation* Attenuation = nullptr;
		bool bLoaded = false;
	};
	FLoadedSound GLoaded[static_cast<int32>(EDCSound::MAX)];

	FLoadedSound& Resolve(EDCSound Sound)
	{
		const int32 Index = static_cast<int32>(Sound);
		FLoadedSound& Entry = GLoaded[Index];
		if (Entry.bLoaded)
		{
			return Entry;
		}
		Entry.bLoaded = true;

		const FSoundSpec& Spec = GSpecs[Index];
		auto Load = [&Entry](const FString& Name)
		{
			const FString Path = FString::Printf(TEXT("/Game/Audio/%s.%s"), *Name, *Name);
			if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn))
			{
				S->AddToRoot(); // lives for the whole session; not referenced by any UPROPERTY
				Entry.Variants.Add(S);
			}
		};
		if (Spec.Variants == 0)
		{
			Load(Spec.BaseName);
		}
		for (int32 i = 1; i <= Spec.Variants; ++i)
		{
			Load(FString::Printf(TEXT("%s_%02d"), Spec.BaseName, i));
		}

		USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
		Att->AddToRoot();
		FSoundAttenuationSettings& S = Att->Attenuation;
		S.bAttenuate = true;
		S.bSpatialize = true;
		S.AttenuationShape = EAttenuationShape::Sphere;
		S.AttenuationShapeExtents = FVector(Spec.FalloffCm * 0.04f, 0.f, 0.f);
		S.FalloffDistance = Spec.FalloffCm;
		S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		S.bAttenuateWithLPF = true;
		S.LPFRadiusMin = Spec.FalloffCm * 0.15f;
		S.LPFRadiusMax = Spec.FalloffCm;
		Entry.Attenuation = Att;
		return Entry;
	}
}

void DCAudio::PlayAt(const UObject* WorldContext, EDCSound Sound, const FVector& Location, float Volume, float PitchScale)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer || Sound == EDCSound::MAX)
	{
		return;
	}
	FLoadedSound& Entry = Resolve(Sound);
	if (Entry.Variants.Num() == 0)
	{
		return;
	}
	USoundBase* Chosen = Entry.Variants[FMath::RandRange(0, Entry.Variants.Num() - 1)];
	const float Jitter = GSpecs[static_cast<int32>(Sound)].PitchJitter;
	UGameplayStatics::PlaySoundAtLocation(WorldContext, Chosen, Location, FRotator::ZeroRotator, Volume,
		PitchScale * FMath::FRandRange(1.f - Jitter, 1.f + Jitter), 0.f, Entry.Attenuation);
}
