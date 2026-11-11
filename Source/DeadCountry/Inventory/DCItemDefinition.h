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

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Item"), GetFName());
	}
};
