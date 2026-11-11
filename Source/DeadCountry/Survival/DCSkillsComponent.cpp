#include "Survival/DCSkillsComponent.h"
#include "Survival/DCSurvivalGameMode.h"
#include "Player/DCPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "DCSkills"

namespace
{
	// Cumulative XP needed to reach levels 1..10.
	const float GThresholds[UDCSkillsComponent::MaxLevel] = { 50.f, 150.f, 300.f, 600.f, 1000.f, 1500.f, 2200.f, 3000.f, 4000.f, 5500.f };
}

UDCSkillsComponent::UDCSkillsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	XP.Init(0.f, static_cast<int32>(EDCSkill::MAX));
}

void UDCSkillsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDCSkillsComponent, XP, COND_OwnerOnly);
}

int32 UDCSkillsComponent::LevelForXP(float Value)
{
	int32 Level = 0;
	while (Level < MaxLevel && Value >= GThresholds[Level])
	{
		++Level;
	}
	return Level;
}

int32 UDCSkillsComponent::GetLevel(EDCSkill Skill) const
{
	return LevelForXP(GetXP(Skill));
}

float UDCSkillsComponent::GetXP(EDCSkill Skill) const
{
	const int32 Index = static_cast<int32>(Skill);
	return XP.IsValidIndex(Index) ? XP[Index] : 0.f;
}

float UDCSkillsComponent::GetProgress(EDCSkill Skill) const
{
	const float Value = GetXP(Skill);
	const int32 Level = LevelForXP(Value);
	if (Level >= MaxLevel)
	{
		return 1.f;
	}
	const float Low = Level > 0 ? GThresholds[Level - 1] : 0.f;
	return FMath::Clamp((Value - Low) / (GThresholds[Level] - Low), 0.f, 1.f);
}

void UDCSkillsComponent::AddXP(EDCSkill Skill, float Amount)
{
	AActor* Owner = GetOwner();
	const int32 Index = static_cast<int32>(Skill);
	if (!Owner || !Owner->HasAuthority() || !XP.IsValidIndex(Index) || Amount <= 0.f)
	{
		return;
	}
	if (const ADCSurvivalGameMode* GM = GetWorld()->GetAuthGameMode<ADCSurvivalGameMode>())
	{
		Amount *= GM->GetSandbox().XPMultiplier;
	}

	const int32 Before = LevelForXP(XP[Index]);
	XP[Index] += Amount;
	const int32 After = LevelForXP(XP[Index]);
	if (After > Before)
	{
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			if (ADCPlayerController* PC = Cast<ADCPlayerController>(Pawn->GetController()))
			{
				PC->Client_ShowMessage(FText::Format(LOCTEXT("LevelUp", "{0} improved to level {1}"), GetSkillName(Skill), After));
			}
		}
	}
}

void UDCSkillsComponent::RestoreXP(const TArray<float>& Saved)
{
	for (int32 i = 0; i < XP.Num() && i < Saved.Num(); ++i)
	{
		XP[i] = FMath::Max(0.f, Saved[i]);
	}
}

FText UDCSkillsComponent::GetSkillName(EDCSkill Skill)
{
	switch (Skill)
	{
	case EDCSkill::Aiming:      return LOCTEXT("Aiming", "Aiming");
	case EDCSkill::Reloading:   return LOCTEXT("Reloading", "Reloading");
	case EDCSkill::ShortBlunt:  return LOCTEXT("ShortBlunt", "Short Blunt");
	case EDCSkill::LongBlunt:   return LOCTEXT("LongBlunt", "Long Blunt");
	case EDCSkill::ShortBlade:  return LOCTEXT("ShortBlade", "Short Blade");
	case EDCSkill::LongBlade:   return LOCTEXT("LongBlade", "Long Blade");
	case EDCSkill::Fitness:     return LOCTEXT("Fitness", "Fitness");
	case EDCSkill::Strength:    return LOCTEXT("Strength", "Strength");
	case EDCSkill::Sneaking:    return LOCTEXT("Sneaking", "Sneaking");
	case EDCSkill::Maintenance: return LOCTEXT("Maintenance", "Maintenance");
	case EDCSkill::Carpentry:   return LOCTEXT("Carpentry", "Carpentry");
	case EDCSkill::Cooking:     return LOCTEXT("Cooking", "Cooking");
	case EDCSkill::FirstAid:    return LOCTEXT("FirstAid", "First Aid");
	default:                    return FText::GetEmpty();
	}
}

#undef LOCTEXT_NAMESPACE
