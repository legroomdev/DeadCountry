#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/DCTypes.h"
#include "DCRegionDefinition.generated.h"

/**
 * One region of the world (e.g. "Coober Creek", "Mt Wallace"). Each points at a level and
 * carries the environment rules that make it play differently.
 * Assets live in /Game/Maps/Definitions (formerly UDCMapDefinition - see CoreRedirects).
 */
UCLASS(BlueprintType)
class DEADCOUNTRY_API UDCRegionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
	EDCBiome Biome = EDCBiome::RedCentre;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region")
	TSoftObjectPtr<UWorld> Level;

	/** Multiplier applied to zombie population in this region. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Region", meta = (ClampMin = 0))
	float ZombieDensity = 1.f;

	/** Hydration drain multiplier - the Red Centre should be brutal, the Snowy Mountains mild. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Environment", meta = (ClampMin = 0))
	float HeatStressMultiplier = 1.f;

	/** Hour of day a brand-new world starts at. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Environment", meta = (ClampMin = 0, ClampMax = 24))
	float StartTimeOfDayHours = 7.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Region"), GetFName());
	}
};
