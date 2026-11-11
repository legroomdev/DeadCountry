#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DCHUD.generated.h"

/**
 * Prototype canvas HUD - zero assets required. Will be replaced by UMG widgets
 * once the systems settle; keep gameplay logic out of here.
 */
UCLASS()
class DEADCOUNTRY_API ADCHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	void DrawCrosshair();
	void DrawLine(const FString& Text, float X, float& Y, const FLinearColor& Color = FLinearColor::White, float Scale = 1.f);
};
