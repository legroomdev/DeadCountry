#include "Health/DCSurvivalComponent.h"
#include "Health/DCHealthComponent.h"
#include "Player/DCCharacter.h"
#include "Survival/DCSurvivalGameState.h"
#include "Survival/DCSkillsComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UDCSurvivalComponent::UDCSurvivalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UDCSurvivalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDCSurvivalComponent, Hydration, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCSurvivalComponent, Energy, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCSurvivalComponent, Fatigue, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDCSurvivalComponent, Stamina, COND_OwnerOnly);
}

void UDCSurvivalComponent::AddHydration(float Amount)
{
	Hydration = FMath::Clamp(Hydration + Amount, 0.f, MaxHydration);
}

void UDCSurvivalComponent::AddEnergy(float Amount)
{
	Energy = FMath::Clamp(Energy + Amount, 0.f, MaxEnergy);
}

void UDCSurvivalComponent::RestoreState(float InHydration, float InEnergy, float InFatigue)
{
	Hydration = FMath::Clamp(InHydration, 0.f, MaxHydration);
	Energy = FMath::Clamp(InEnergy, 0.f, MaxEnergy);
	Fatigue = FMath::Clamp(InFatigue, 0.f, 100.f);
}

void UDCSurvivalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ADCCharacter* Character = Cast<ADCCharacter>(GetOwner());
	if (!Character || !Character->HasAuthority() || Character->IsDead())
	{
		return;
	}

	// Convert this real-time tick into in-game hours (includes time skips while sleeping).
	const ADCSurvivalGameState* GS = GetWorld()->GetGameState<ADCSurvivalGameState>();
	const float GameHours = DeltaTime * (GS ? GS->GetGameHoursPerSecond() : 24.f / 3600.f);

	const bool bSleeping = Character->IsSleeping();
	const bool bSprinting = Character->IsSprinting() && Character->GetVelocity().SizeSquared2D() > 100.f;
	const float Exertion = bSprinting ? 2.5f : (bSleeping ? 0.6f : 1.f);

	Hydration = FMath::Max(0.f, Hydration - HydrationDrainPerHour * HeatStressMultiplier * Exertion * GameHours);
	Energy = FMath::Max(0.f, Energy - EnergyDrainPerHour * Exertion * GameHours);

	if (bSleeping)
	{
		Fatigue = FMath::Max(0.f, Fatigue - FatigueRecoveryPerHour * GameHours);
	}
	else
	{
		Fatigue = FMath::Min(100.f, Fatigue + FatigueGainPerHour * (bSprinting ? 1.5f : 1.f) * GameHours);
	}

	// Stamina is moment-to-moment, so real time. Being tired halves recovery; Fitness helps both ways.
	const int32 Fitness = Character->GetSkillsComponent() ? Character->GetSkillsComponent()->GetLevel(EDCSkill::Fitness) : 0;
	if (bSprinting)
	{
		Stamina = FMath::Max(0.f, Stamina - SprintStaminaDrainPerSecond * (1.f - 0.04f * Fitness) * DeltaTime);
	}
	else
	{
		const float Regen = StaminaRegenPerSecond * (IsTired() ? 0.5f : 1.f) * (1.f + 0.06f * Fitness);
		Stamina = FMath::Min(MaxStamina, Stamina + Regen * DeltaTime);
	}

	if (UDCHealthComponent* Health = Character->GetHealthComponent())
	{
		if (Hydration <= 0.f)
		{
			Health->ApplyDamageToPart(EDCBodyPart::Thorax, DepletedDamagePerHour * GameHours, nullptr);
		}
		if (Energy <= 0.f)
		{
			Health->ApplyDamageToPart(EDCBodyPart::Stomach, DepletedDamagePerHour * GameHours, nullptr);
		}
	}
}
