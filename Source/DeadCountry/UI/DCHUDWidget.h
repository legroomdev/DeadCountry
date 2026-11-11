#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DCHUDWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;
class ADCCharacter;
class ADCPlayerController;
class ADCPlayerState;
class ADCSurvivalGameState;

/**
 * The in-game HUD, built entirely in C++ (no widget blueprint needed).
 *  - Status icons ("moodles"): only what matters right now, coloured by severity.
 *  - Body silhouette: each part tinted by health, flashing red while bleeding.
 *  - Weapon/ammo, stamina bar, clock, survivor, interaction prompt, server messages.
 *  - Sleep, death and inventory overlays.
 * Restyle freely later (or replace with a designer-made UMG blueprint using the same data).
 */
UCLASS()
class DEADCOUNTRY_API UDCHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FStatusIcon
	{
		FString Label;
		int32 Level = 1; // 1 mild .. 4 critical
	};

	void BuildTree();
	UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold = false) const;
	UBorder* MakeBox(const FLinearColor& Color, float Radius, const FMargin& Padding) const;
	class UCanvasPanelSlot* Place(UWidget* Widget, const FVector2D& AnchorMin, const FVector2D& AnchorMax,
		const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment, bool bAutoSize);

	void UpdateStatus(const ADCCharacter* Character, const ADCPlayerController* PC);
	void UpdateBody(const ADCCharacter* Character);
	void UpdateWeapon(const ADCCharacter* Character);
	void UpdateInventory(const ADCCharacter* Character, const ADCPlayerController* PC);
	void GatherStatusIcons(const ADCCharacter* Character, TArray<FStatusIcon>& Out) const;

	static FLinearColor SeverityColor(int32 Level);
	static FLinearColor HealthColor(float Fraction);

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;

	UPROPERTY() TObjectPtr<UTextBlock> ClockText;
	UPROPERTY() TObjectPtr<UTextBlock> SurvivorText;
	UPROPERTY() TObjectPtr<UBorder> Crosshair;
	UPROPERTY() TObjectPtr<UTextBlock> PromptText;
	UPROPERTY() TObjectPtr<UTextBlock> MessageText;

	UPROPERTY() TObjectPtr<UVerticalBox> StatusColumn;
	UPROPERTY() TArray<TObjectPtr<UBorder>> StatusBoxes;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> StatusTexts;

	UPROPERTY() TArray<TObjectPtr<UBorder>> BodyParts;      // indexed by EDCBodyPart
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> BodyMarks;
	UPROPERTY() TObjectPtr<UTextBlock> BodySummary;

	UPROPERTY() TObjectPtr<UBorder> StaminaBack;
	UPROPERTY() TObjectPtr<UBorder> StaminaFill;

	UPROPERTY() TObjectPtr<UTextBlock> WeaponNameText;
	UPROPERTY() TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY() TObjectPtr<UTextBlock> ReserveText;

	UPROPERTY() TObjectPtr<UBorder> InventoryPanel;
	UPROPERTY() TObjectPtr<UTextBlock> InventoryHeader;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> InventoryRows;

	UPROPERTY() TObjectPtr<UBorder> SleepOverlay;
	UPROPERTY() TObjectPtr<UTextBlock> SleepText;

	UPROPERTY() TObjectPtr<UBorder> DeathOverlay;
	UPROPERTY() TObjectPtr<UTextBlock> DeathBody;

	float Pulse = 0.f;
};
