#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DCHUD.generated.h"

class UDCHUDWidget;

/** Owns the on-screen UMG HUD for the local player. */
UCLASS()
class DEADCOUNTRY_API ADCHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	TObjectPtr<UDCHUDWidget> Widget;
};
