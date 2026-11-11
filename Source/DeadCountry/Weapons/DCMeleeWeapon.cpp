#include "Weapons/DCMeleeWeapon.h"
#include "DeadCountry.h"
#include "AI/DCZombie.h"
#include "Core/DCAudio.h"
#include "Health/DCSurvivalComponent.h"
#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "Player/DCCharacter.h"
#include "Survival/DCSkillsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	// Held-pose keyframes (camera space): resting over the shoulder, wound up, follow-through.
	struct FPoseKey { FVector Loc; FRotator Rot; };
	const FPoseKey Rest     { FVector(30.f, 17.f, -20.f), FRotator(62.f, -12.f, -8.f) };
	const FPoseKey WindUp   { FVector(24.f, 26.f, -6.f),  FRotator(95.f, 25.f, -35.f) };
	const FPoseKey Strike   { FVector(44.f, 0.f, -12.f),  FRotator(-5.f, -30.f, -80.f) };
	const FPoseKey Follow   { FVector(36.f, -20.f, -22.f), FRotator(-20.f, -70.f, -95.f) };

	FPoseKey Blend(const FPoseKey& A, const FPoseKey& B, float T)
	{
		const float S = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T, 0.f, 1.f));
		return { FMath::Lerp(A.Loc, B.Loc, S), FMath::Lerp(A.Rot, B.Rot, S) };
	}
}

ADCMeleeWeapon::ADCMeleeWeapon()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->CastShadow = false;
	RootComponent = Mesh;
}

void ADCMeleeWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCMeleeWeapon, Condition);
}

void ADCMeleeWeapon::OnRep_Item()
{
	if (Item && !Item->WorldMesh.IsNull())
	{
		Mesh->SetStaticMesh(Item->WorldMesh.LoadSynchronous());
	}
}

void ADCMeleeWeapon::InitFromEntry(const FDCInventoryEntry& Entry)
{
	Super::InitFromEntry(Entry);
	Condition = Entry.Condition >= 0.f ? Entry.Condition : (Item ? Item->MaxCondition : -1.f);
}

void ADCMeleeWeapon::GetInstanceData(float& OutCondition, int32& OutLoadedAmmo) const
{
	OutCondition = Condition;
	OutLoadedAmmo = -1;
}

float ADCMeleeWeapon::ComputeSwingSeconds() const
{
	const ADCCharacter* Character = GetOwningCharacter();
	float Seconds = Item ? Item->SwingSeconds : 0.8f;
	if (Character && Item)
	{
		Seconds *= 1.f - 0.03f * Character->GetSkillsComponent()->GetLevel(Item->WeaponSkill);
		if (Character->GetSurvivalComponent()->GetStamina() < 20.f)
		{
			Seconds *= 1.3f; // exhausted swings are slow and sloppy
		}
	}
	return FMath::Max(0.25f, Seconds);
}

bool ADCMeleeWeapon::IsBusy() const
{
	return GetWorld()->GetTimeSeconds() < SwingStartTime + SwingDuration;
}

void ADCMeleeWeapon::StartPrimary()
{
	ADCCharacter* Character = GetOwningCharacter();
	if (!Character || Character->IsDead() || IsBusy() || !Item)
	{
		return;
	}
	if (Character->GetSurvivalComponent()->GetStamina() < MinStaminaToSwing)
	{
		return; // too winded to swing
	}

	// Animate immediately for the swinger; the server decides what it hits.
	SwingDuration = ComputeSwingSeconds();
	SwingStartTime = GetWorld()->GetTimeSeconds();
	DCAudio::PlayAt(this, EDCSound::MeleeSwing, GetActorLocation(), 0.8f, Item->MeleeArcDegrees > 80.f ? 0.85f : 1.f);

	FVector ViewLocation;
	FRotator ViewRotation;
	Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	Server_Swing(ViewLocation, ViewRotation.Vector());
}

void ADCMeleeWeapon::Server_Swing_Implementation(FVector_NetQuantize ViewLocation, FVector_NetQuantizeNormal ViewDirection)
{
	ADCCharacter* Character = GetOwningCharacter();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Character || Character->IsDead() || !Item || Now < ServerSwingEndTime - 0.05f)
	{
		return;
	}
	UDCSurvivalComponent* Survival = Character->GetSurvivalComponent();
	if (Survival->GetStamina() < MinStaminaToSwing)
	{
		return;
	}

	const float Duration = ComputeSwingSeconds();
	ServerSwingEndTime = Now + Duration;
	Survival->ConsumeStamina(Item->SwingStaminaCost);

	FVector Eyes;
	FRotator EyeRot;
	Character->GetActorEyesViewPoint(Eyes, EyeRot);
	StrikeOrigin = FVector::DistSquared(Eyes, ViewLocation) < FMath::Square(150.f) ? FVector(ViewLocation) : Eyes;
	StrikeDirection = ViewDirection;
	GetWorldTimerManager().SetTimer(StrikeTimer, this, &ADCMeleeWeapon::PerformStrike, Duration * StrikeFraction, false);

	Multicast_SwingStarted(Duration);

	// Swinging is quiet, but not silent.
	MakeNoise(0.25f, Character, StrikeOrigin, 500.f);
}

void ADCMeleeWeapon::Multicast_SwingStarted_Implementation(float Duration)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		return; // the swinger already animated
	}
	SwingDuration = Duration;
	SwingStartTime = GetWorld()->GetTimeSeconds();
	DCAudio::PlayAt(this, EDCSound::MeleeSwing, GetActorLocation(), 0.8f);
}

void ADCMeleeWeapon::PerformStrike()
{
	ADCCharacter* Character = GetOwningCharacter();
	if (!Character || Character->IsDead() || !Item)
	{
		return;
	}
	UWorld* World = GetWorld();
	UDCSkillsComponent* Skills = Character->GetSkillsComponent();
	const int32 SkillLevel = Skills->GetLevel(Item->WeaponSkill);
	const int32 Strength = Skills->GetLevel(EDCSkill::Strength);

	// Sweep the arc with a fan of sphere traces; collect each zombie once (nearest hit wins).
	struct FTarget { ADCZombie* Zombie; FHitResult Hit; };
	TArray<FTarget> Targets;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCMeleeSweep), false);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Character);
	const int32 Rays = 9;
	for (int32 i = 0; i < Rays; ++i)
	{
		const float Yaw = FMath::Lerp(-Item->MeleeArcDegrees * 0.5f, Item->MeleeArcDegrees * 0.5f, i / float(Rays - 1));
		const FVector Dir = StrikeDirection.RotateAngleAxis(Yaw, FVector::UpVector);
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, StrikeOrigin, StrikeOrigin + Dir * Item->MeleeReach, FQuat::Identity,
			DC_TRACE_WEAPON, FCollisionShape::MakeSphere(14.f), Params))
		{
			ADCZombie* Zombie = Cast<ADCZombie>(Hit.GetActor());
			if (Zombie && !Zombie->IsDead())
			{
				FTarget* Existing = Targets.FindByPredicate([Zombie](const FTarget& T) { return T.Zombie == Zombie; });
				if (!Existing)
				{
					Targets.Add({ Zombie, Hit });
				}
				else if (Hit.Distance < Existing->Hit.Distance)
				{
					Existing->Hit = Hit;
				}
			}
		}
	}
	UE_LOG(LogDC, Verbose, TEXT("Melee strike: %d target(s) in arc"), Targets.Num());
	Targets.Sort([](const FTarget& A, const FTarget& B) { return A.Hit.Distance < B.Hit.Distance; });
	Targets.SetNum(FMath::Min(Targets.Num(), Item->MeleeMaxTargets));

	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		const FTarget& T = Targets[i];
		// Later targets in the same swing take less (the swing loses momentum).
		float Damage = Item->MeleeDamage * (1.f + 0.08f * SkillLevel + 0.04f * Strength) * FMath::FRandRange(0.85f, 1.15f) * (i == 0 ? 1.f : 0.6f);
		const bool bCrit = FMath::FRand() < Item->CritChance + 0.015f * SkillLevel;
		if (bCrit)
		{
			Damage *= 2.f;
		}
		const bool bKnockdown = FMath::FRand() < Item->KnockdownChance + 0.03f * Strength;
		const FVector Dir = (T.Zombie->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
		T.Zombie->ApplyMeleeHit(Damage, Dir, T.Hit, bKnockdown, Character->GetController());
		Multicast_Impact(T.Hit.ImpactPoint, Item->bBladed);
		UE_LOG(LogDC, Log, TEXT("Melee: %s hit %s (%s) for %.0f%s%s"), *Item->DisplayName.ToString(), *T.Zombie->GetName(),
			*T.Hit.BoneName.ToString(), Damage, bCrit ? TEXT(" CRIT") : TEXT(""), bKnockdown ? TEXT(" knockdown") : TEXT(""));

		Skills->AddXP(Item->WeaponSkill, 3.f);
		Skills->AddXP(EDCSkill::Strength, 1.f);

		// Wear: each hit may cost a point of condition (Maintenance slows it).
		if (Condition > 0.f)
		{
			const float LossChance = Item->ConditionLossChance * (1.f - 0.07f * Skills->GetLevel(EDCSkill::Maintenance));
			if (FMath::FRand() < LossChance)
			{
				Condition = FMath::Max(0.f, Condition - 1.f);
				Skills->AddXP(EDCSkill::Maintenance, 1.f);
			}
		}
	}

	if (Condition == 0.f)
	{
		Character->HandleEquippedBroken(); // destroys this actor
	}
}

void ADCMeleeWeapon::Multicast_Impact_Implementation(FVector_NetQuantize Location, bool bBladed)
{
	DCAudio::PlayAt(this, bBladed ? EDCSound::MeleeHitBlade : EDCSound::MeleeHitBlunt, Location);
}

void ADCMeleeWeapon::GetHeldPose(float AimAlpha, FVector& OutLocation, FRotator& OutRotation) const
{
	const float T = (GetWorld()->GetTimeSeconds() - SwingStartTime) / FMath::Max(0.05f, SwingDuration);
	FPoseKey Pose = Rest;
	if (T >= 0.f && T < 1.f)
	{
		// Wind-up -> strike -> follow-through -> back to rest.
		if (T < 0.25f)       Pose = Blend(Rest, WindUp, T / 0.25f);
		else if (T < 0.45f)  Pose = Blend(WindUp, Strike, (T - 0.25f) / 0.2f);
		else if (T < 0.6f)   Pose = Blend(Strike, Follow, (T - 0.45f) / 0.15f);
		else                 Pose = Blend(Follow, Rest, (T - 0.6f) / 0.4f);
	}
	OutLocation = Pose.Loc;
	OutRotation = Pose.Rot;
}

FString ADCMeleeWeapon::GetHudPrimary() const
{
	if (!Item || Item->MaxCondition <= 0.f || Condition < 0.f)
	{
		return FString();
	}
	return FString::Printf(TEXT("Condition %d%%"), FMath::RoundToInt(100.f * Condition / Item->MaxCondition));
}

FString ADCMeleeWeapon::GetHudSecondary() const
{
	return TEXT("[LMB] swing   [RMB] shove");
}

FQuat ADCMeleeWeapon::GetArmsReferenceRotation() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetControlRotation().Quaternion() : GetActorQuat();
}

float ADCMeleeWeapon::GetHudBarFraction() const
{
	return (Item && Item->MaxCondition > 0.f && Condition >= 0.f) ? Condition / Item->MaxCondition : -1.f;
}
