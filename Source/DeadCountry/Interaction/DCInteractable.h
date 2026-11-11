#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DCInteractable.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UDCInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the player can look at and press Interact on: loose items, eskies, car boots, doors, extract switches.
 * Interact is only ever called on the server.
 */
class DEADCOUNTRY_API IDCInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText(APawn* InstigatorPawn) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* InstigatorPawn) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(APawn* InstigatorPawn);
};
