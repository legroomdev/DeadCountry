#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/DCTypes.h"
#include "DCItemDefinition.generated.h"

class UStaticMesh;
class UTexture2D;

/**
 * Static data for one kind of item (Water Bottle, Bandage, 5.56 round...).
 * Create these as Data Assets under /Game/Items.
 */
UCLASS(BlueprintType)
class DEADCOUNTRY_API UDCItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EDCItemCategory Category = EDCItemCategory::Misc;

	/** Cells occupied in a grid inventory (Tarkov-style). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1, ClampMax = 10))
	FIntPoint GridSize = FIntPoint(1, 1);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 MaxStack = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 0))
	float WeightKg = 0.1f;

	/** Trader / stash value in dollars. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 0))
	int32 BaseValue = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	TSoftObjectPtr<UTexture2D> Icon;

	// --- Use effects (consumables) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Use")
	bool bConsumable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Use", meta = (EditCondition = "bConsumable"))
	float HealAmount = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Use", meta = (EditCondition = "bConsumable"))
	bool bStopsBleeding = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Use", meta = (EditCondition = "bConsumable"))
	bool bSplintsFracture = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Use", meta = (EditCondition = "bConsumable"))
	float HydrationRestore = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Use", meta = (EditCondition = "bConsumable"))
	float EnergyRestore = 0.f;

	// --- Equipping (weapons, tools) ---

	/** Actor spawned in the survivor's hands when this item is equipped. None = not equippable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip")
	TSubclassOf<class ADCEquippable> EquipClass;

	/** Durability. 0 = never wears out. Each instance tracks its own condition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equip", meta = (ClampMin = 0))
	float MaxCondition = 0.f;

	// --- Melee (used when EquipClass is a melee weapon) ---

	/** The skill that levels up (and boosts) when fighting with this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	EDCSkill WeaponSkill = EDCSkill::LongBlunt;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	bool bBladed = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MeleeDamage = 25.f;

	/** How far the swing reaches from the eyes (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MeleeReach = 160.f;

	/** Width of the swing (degrees). Wide swings can hit several zombies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float MeleeArcDegrees = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = 1))
	int32 MeleeMaxTargets = 2;

	/** Full swing time at skill 0 (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float SwingSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	float SwingStaminaCost = 9.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = 0, ClampMax = 1))
	float KnockdownChance = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = 0, ClampMax = 1))
	float CritChance = 0.08f;

	/** Chance each hit wears the weapon by one point (Maintenance skill reduces it). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = 0, ClampMax = 1))
	float ConditionLossChance = 0.2f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Item"), GetFName());
	}
};
