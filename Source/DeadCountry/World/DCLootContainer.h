#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DCInteractable.h"
#include "DCLootContainer.generated.h"

class UStaticMeshComponent;
class UDCInventoryComponent;
class UDCItemDefinition;

USTRUCT(BlueprintType)
struct DEADCOUNTRY_API FDCLootRoll
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDCItemDefinition> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 0, ClampMax = 1))
	float Chance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 1))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = 1))
	int32 MaxCount = 1;
};

/**
 * Esky, servo shelf, ute tray, glovebox, dead player's body...
 * Rolls its loot table when the raid starts. Interact takes everything that fits
 * (a proper two-panel loot UI comes later).
 */
UCLASS()
class DEADCOUNTRY_API ADCLootContainer : public AActor, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCLootContainer();

	virtual FText GetInteractionText_Implementation(APawn* InstigatorPawn) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	UDCInventoryComponent* GetInventoryComponent() const { return Inventory; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	void RollLoot();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Loot")
	FText ContainerName;

	/** Remove the actor once it's been emptied (used for player bodies). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	bool bDestroyWhenEmpty = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UDCInventoryComponent> Inventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FDCLootRoll> LootTable;
};
