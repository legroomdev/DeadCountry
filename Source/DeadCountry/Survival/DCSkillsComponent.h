#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DCTypes.h"
#include "DCSkillsComponent.generated.h"

/**
 * Zomboid-style skills, levelled 0-10 by doing things. Server-authoritative, replicated to the owner.
 * Effects are read where they apply (melee damage/speed, aim spread, reload speed, stamina, carry weight...).
 */
UCLASS(ClassGroup = (DC), meta = (BlueprintSpawnableComponent))
class DEADCOUNTRY_API UDCSkillsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCSkillsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static constexpr int32 MaxLevel = 10;

	/** Server: award experience (scaled by the world's XP multiplier); announces level-ups to the owner. */
	void AddXP(EDCSkill Skill, float Amount);

	UFUNCTION(BlueprintPure, Category = "Skills")
	int32 GetLevel(EDCSkill Skill) const;

	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetXP(EDCSkill Skill) const;

	/** 0..1 progress toward the next level. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetProgress(EDCSkill Skill) const;

	static FText GetSkillName(EDCSkill Skill);
	static int32 LevelForXP(float XP);

	/** World save support. */
	const TArray<float>& GetAllXP() const { return XP; }
	void RestoreXP(const TArray<float>& Saved);

protected:
	UPROPERTY(Replicated)
	TArray<float> XP;
};
