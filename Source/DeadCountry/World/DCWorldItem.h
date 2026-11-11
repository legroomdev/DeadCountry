#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DCInteractable.h"
#include "DCWorldItem.generated.h"

class UStaticMeshComponent;
class UDCItemDefinition;

/** A loose item lying in the world. Walk up, press Interact, it goes in your bag. */
UCLASS()
class DEADCOUNTRY_API ADCWorldItem : public AActor, public IDCInteractable
{
	GENERATED_BODY()

public:
	ADCWorldItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual FText GetInteractionText_Implementation(APawn* InstigatorPawn) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item")
	void SetItem(UDCItemDefinition* NewItem, int32 NewCount);

	static ADCWorldItem* SpawnDropped(UWorld* World, UDCItemDefinition* Item, int32 Count, const FVector& Location, const FRotator& Rotation,
		float InCondition = -1.f, int32 InLoadedAmmo = -1);

	UDCItemDefinition* GetItem() const { return Item; }
	int32 GetCount() const { return Count; }
	float GetCondition() const { return Condition; }
	int32 GetLoadedAmmo() const { return LoadedAmmo; }

	/** Per-instance state carried by a dropped weapon/tool (-1 = fresh from the item definition). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float Condition = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 LoadedAmmo = -1;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Item, Category = "Item")
	TObjectPtr<UDCItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Item", meta = (ClampMin = 1))
	int32 Count = 1;

	UFUNCTION()
	void OnRep_Item();

	void RefreshMesh();
};
