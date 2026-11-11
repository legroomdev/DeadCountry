#include "UI/DCHUDWidget.h"
#include "Player/DCCharacter.h"
#include "Player/DCPlayerController.h"
#include "Health/DCHealthComponent.h"
#include "Health/DCSurvivalComponent.h"
#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "Weapons/DCWeapon.h"
#include "Survival/DCPlayerState.h"
#include "Survival/DCSurvivalGameState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

namespace DCUI
{
	const FLinearColor Dust(0.93f, 0.80f, 0.60f);
	const FLinearColor Dim(0.75f, 0.75f, 0.75f, 0.9f);
	const FLinearColor Night(0.62f, 0.72f, 1.f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.55f);
	const int32 MaxStatusIcons = 10;
	const int32 MaxInventoryRows = 24;

	// Body silhouette layout (x, y, w, h) - viewed from behind, so the left arm is on the left.
	struct FPartRect { float X, Y, W, H; };
	const FPartRect PartRects[] = {
		{ 37.f,   0.f, 26.f, 26.f }, // Head
		{ 28.f,  30.f, 44.f, 36.f }, // Thorax
		{ 30.f,  70.f, 40.f, 26.f }, // Stomach
		{  6.f,  30.f, 18.f, 64.f }, // LeftArm
		{ 76.f,  30.f, 18.f, 64.f }, // RightArm
		{ 30.f, 100.f, 18.f, 74.f }, // LeftLeg
		{ 52.f, 100.f, 18.f, 74.f }, // RightLeg
	};
	const FVector2D BodyOrigin(26.f, -250.f); // from the bottom-left corner
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TSharedRef<SWidget> UDCHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

UTextBlock* UDCHUDWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold) const
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetShadowOffset(FVector2D(1.f, 1.f));
	Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	return Text;
}

UBorder* UDCHUDWidget::MakeBox(const FLinearColor& Color, float Radius, const FMargin& BoxPadding) const
{
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.TintColor = FSlateColor(FLinearColor::White);
	Box->SetBrush(Brush);
	Box->SetBrushColor(Color);
	Box->SetPadding(BoxPadding);
	return Box;
}

UCanvasPanelSlot* UDCHUDWidget::Place(UWidget* Widget, const FVector2D& AnchorMin, const FVector2D& AnchorMax,
	const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment, bool bAutoSize)
{
	UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Widget);
	CanvasSlot->SetAnchors(FAnchors(AnchorMin.X, AnchorMin.Y, AnchorMax.X, AnchorMax.Y));
	CanvasSlot->SetAlignment(Alignment);
	CanvasSlot->SetAutoSize(bAutoSize);
	CanvasSlot->SetPosition(Position);
	if (!bAutoSize)
	{
		CanvasSlot->SetSize(Size);
	}
	return CanvasSlot;
}

void UDCHUDWidget::BuildTree()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;
	const FVector2D TopCentre(0.5f, 0.f), Centre(0.5f, 0.5f), BottomLeft(0.f, 1.f), BottomRight(1.f, 1.f), RightMid(1.f, 0.5f);

	// Clock + survivor
	ClockText = MakeText(20, DCUI::Dust, true);
	Place(ClockText, TopCentre, TopCentre, FVector2D(0.f, 14.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.f), true);
	SurvivorText = MakeText(12, DCUI::Dim);
	Place(SurvivorText, TopCentre, TopCentre, FVector2D(0.f, 44.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.f), true);

	// Crosshair, prompt, messages
	Crosshair = MakeBox(FLinearColor(1.f, 1.f, 1.f, 0.85f), 2.f, FMargin(0.f));
	Place(Crosshair, Centre, Centre, FVector2D::ZeroVector, FVector2D(4.f, 4.f), FVector2D(0.5f, 0.5f), false);
	PromptText = MakeText(15, DCUI::Dust, true);
	Place(PromptText, Centre, Centre, FVector2D(0.f, 34.f), FVector2D::ZeroVector, FVector2D(0.5f, 0.f), true);
	MessageText = MakeText(17, FLinearColor::White, true);
	Place(MessageText, FVector2D(0.5f, 0.3f), FVector2D(0.5f, 0.3f), FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(0.5f, 0.5f), true);

	// Status icons, right side
	StatusColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	Place(StatusColumn, RightMid, RightMid, FVector2D(-22.f, -60.f), FVector2D::ZeroVector, FVector2D(1.f, 0.5f), true);
	for (int32 i = 0; i < DCUI::MaxStatusIcons; ++i)
	{
		UBorder* Box = MakeBox(FLinearColor::Gray, 8.f, FMargin(12.f, 5.f));
		UTextBlock* Text = MakeText(13, FLinearColor(0.05f, 0.05f, 0.05f), true);
		Box->SetContent(Text);
		if (UVerticalBoxSlot* VSlot = StatusColumn->AddChildToVerticalBox(Box))
		{
			VSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			VSlot->SetHorizontalAlignment(HAlign_Right);
		}
		Box->SetVisibility(ESlateVisibility::Collapsed);
		StatusBoxes.Add(Box);
		StatusTexts.Add(Text);
	}

	// Body silhouette, bottom left
	BodySummary = MakeText(12, DCUI::Dim, true);
	Place(BodySummary, BottomLeft, BottomLeft, DCUI::BodyOrigin + FVector2D(0.f, -24.f), FVector2D::ZeroVector, FVector2D(0.f, 0.f), true);
	for (const DCUI::FPartRect& R : DCUI::PartRects)
	{
		UBorder* Part = MakeBox(FLinearColor(0.4f, 0.75f, 0.4f, 0.85f), 5.f, FMargin(0.f));
		Part->SetHorizontalAlignment(HAlign_Center);
		Part->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Mark = MakeText(12, FLinearColor::White, true);
		Part->SetContent(Mark);
		Place(Part, BottomLeft, BottomLeft, DCUI::BodyOrigin + FVector2D(R.X, R.Y), FVector2D(R.W, R.H), FVector2D(0.f, 0.f), false);
		BodyParts.Add(Part);
		BodyMarks.Add(Mark);
	}

	// Stamina bar, bottom centre (only shown when not full)
	const FVector2D BottomCentre(0.5f, 1.f);
	StaminaBack = MakeBox(FLinearColor(0.f, 0.f, 0.f, 0.5f), 3.f, FMargin(0.f));
	Place(StaminaBack, BottomCentre, BottomCentre, FVector2D(0.f, -30.f), FVector2D(220.f, 6.f), FVector2D(0.5f, 0.5f), false);
	StaminaFill = MakeBox(FLinearColor(0.9f, 0.85f, 0.6f, 0.9f), 3.f, FMargin(0.f));
	Place(StaminaFill, BottomCentre, BottomCentre, FVector2D(-110.f, -30.f), FVector2D(220.f, 6.f), FVector2D(0.f, 0.5f), false);

	// Weapon, bottom right
	WeaponNameText = MakeText(12, DCUI::Dim);
	WeaponNameText->SetJustification(ETextJustify::Right);
	Place(WeaponNameText, BottomRight, BottomRight, FVector2D(-28.f, -96.f), FVector2D::ZeroVector, FVector2D(1.f, 1.f), true);
	AmmoText = MakeText(30, DCUI::Dust, true);
	AmmoText->SetJustification(ETextJustify::Right);
	Place(AmmoText, BottomRight, BottomRight, FVector2D(-28.f, -50.f), FVector2D::ZeroVector, FVector2D(1.f, 1.f), true);
	ReserveText = MakeText(13, DCUI::Dim);
	ReserveText->SetJustification(ETextJustify::Right);
	Place(ReserveText, BottomRight, BottomRight, FVector2D(-28.f, -28.f), FVector2D::ZeroVector, FVector2D(1.f, 1.f), true);

	// Inventory panel (Tab)
	InventoryPanel = MakeBox(DCUI::Panel, 8.f, FMargin(16.f, 12.f));
	UVerticalBox* InvList = WidgetTree->ConstructWidget<UVerticalBox>();
	InventoryPanel->SetContent(InvList);
	InventoryHeader = MakeText(15, DCUI::Dust, true);
	InvList->AddChildToVerticalBox(InventoryHeader);
	UTextBlock* Hint = MakeText(11, DCUI::Dim);
	Hint->SetText(NSLOCTEXT("DCHUD", "InvHint", "Press 1-9 to use an item"));
	InvList->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 2.f, 0.f, 8.f));
	for (int32 i = 0; i < DCUI::MaxInventoryRows; ++i)
	{
		UTextBlock* Row = MakeText(13, FLinearColor::White);
		InvList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 1.f));
		InventoryRows.Add(Row);
	}
	Place(InventoryPanel, TopCentre, TopCentre, FVector2D(110.f, 110.f), FVector2D::ZeroVector, FVector2D(0.f, 0.f), true);
	InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);

	// Full-screen overlays
	auto MakeOverlay = [this](const FLinearColor& Color, UTextBlock*& OutBody, const FText& Title, const FLinearColor& TitleColor)
	{
		UBorder* Overlay = MakeBox(Color, 0.f, FMargin(0.f));
		Overlay->SetHorizontalAlignment(HAlign_Center);
		Overlay->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
		Overlay->SetContent(Stack);
		UTextBlock* TitleText = MakeText(40, TitleColor, true);
		TitleText->SetText(Title);
		TitleText->SetJustification(ETextJustify::Center);
		Stack->AddChildToVerticalBox(TitleText)->SetHorizontalAlignment(HAlign_Center);
		OutBody = MakeText(16, DCUI::Dust);
		OutBody->SetJustification(ETextJustify::Center);
		Stack->AddChildToVerticalBox(OutBody)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(Overlay);
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		OverlaySlot->SetOffsets(FMargin(0.f));
		Overlay->SetVisibility(ESlateVisibility::Collapsed);
		return Overlay;
	};
	UTextBlock* SleepBody = nullptr;
	SleepOverlay = MakeOverlay(FLinearColor(0.f, 0.f, 0.02f, 0.94f), SleepBody, NSLOCTEXT("DCHUD", "Sleeping", "Sleeping..."), DCUI::Night);
	SleepText = SleepBody;
	UTextBlock* DeathText = nullptr;
	DeathOverlay = MakeOverlay(FLinearColor(0.f, 0.f, 0.f, 0.8f), DeathText, NSLOCTEXT("DCHUD", "Died", "YOU DIED"), FLinearColor(0.85f, 0.12f, 0.1f));
	DeathBody = DeathText;
}

// ---------------------------------------------------------------------------
// Per-frame update
// ---------------------------------------------------------------------------

void UDCHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Pulse += InDeltaTime;

	const ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer());
	const ADCPlayerState* PS = PC ? PC->GetPlayerState<ADCPlayerState>() : nullptr;
	const ADCSurvivalGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADCSurvivalGameState>() : nullptr;
	const ADCCharacter* Character = PC ? PC->GetPawn<ADCCharacter>() : nullptr;

	// Top: clock + survivor
	if (GS)
	{
		ClockText->SetText(FText::FromString(GS->GetClockString()));
		ClockText->SetColorAndOpacity(FSlateColor(GS->IsNight() ? DCUI::Night : DCUI::Dust));
	}
	SurvivorText->SetText((PS && GS && !PS->SurvivorName.IsEmpty())
		? FText::FromString(FString::Printf(TEXT("%s  -  %.1f days survived"), *PS->SurvivorName, PS->GetDaysSurvived(GS->GetTotalGameHours())))
		: FText::GetEmpty());
	MessageText->SetText(PC ? PC->GetActiveMessage() : FText::GetEmpty());

	// Death overlay
	const bool bDead = PS && PS->bSurvivorDead && !Character;
	DeathOverlay->SetVisibility(bDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bDead)
	{
		DeathBody->SetText(FText::FromString(FString::Printf(
			TEXT("%s survived %.1f days and killed %d zombies.\nTheir body is still out there, with everything they carried.\n\nPress ENTER to start again as a new survivor"),
			*PS->SurvivorName, PS->LastDaysSurvived, PS->ZombieKills)));
	}

	// Sleep overlay
	const bool bSleeping = Character && Character->IsSleeping();
	SleepOverlay->SetVisibility(bSleeping ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bSleeping && GS)
	{
		SleepText->SetText(FText::FromString(FString::Printf(TEXT("%s\nFatigue %.0f\n\n[Z] wake up"),
			*GS->GetClockString(), Character->GetSurvivalComponent()->GetFatigue())));
	}

	const bool bShowGameplay = Character && !bSleeping;
	const ESlateVisibility GameplayVis = bShowGameplay ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	for (UWidget* W : TArray<UWidget*>{ PromptText, StatusColumn, BodySummary, WeaponNameText, AmmoText, ReserveText })
	{
		W->SetVisibility(GameplayVis);
	}
	for (UBorder* Part : BodyParts)
	{
		Part->SetVisibility(GameplayVis);
	}
	if (!bShowGameplay)
	{
		Crosshair->SetVisibility(ESlateVisibility::Collapsed);
		StaminaBack->SetVisibility(ESlateVisibility::Collapsed);
		StaminaFill->SetVisibility(ESlateVisibility::Collapsed);
		InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Crosshair->SetVisibility(Character->GetAimAlpha() < 0.5f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const FText Prompt = Character->GetFocusedInteractionText();
	PromptText->SetText(Prompt.IsEmpty() ? FText::GetEmpty() : FText::Format(NSLOCTEXT("DCHUD", "Prompt", "[F] {0}"), Prompt));

	UpdateStatus(Character, PC);
	UpdateBody(Character);
	UpdateWeapon(Character);
	UpdateInventory(Character, PC);
}

FLinearColor UDCHUDWidget::SeverityColor(int32 Level)
{
	switch (FMath::Clamp(Level, 1, 4))
	{
	case 1:  return FLinearColor(0.93f, 0.85f, 0.45f, 0.92f);
	case 2:  return FLinearColor(0.98f, 0.62f, 0.25f, 0.94f);
	case 3:  return FLinearColor(0.95f, 0.32f, 0.2f, 0.96f);
	default: return FLinearColor(0.75f, 0.06f, 0.06f, 0.98f);
	}
}

FLinearColor UDCHUDWidget::HealthColor(float Fraction)
{
	const FLinearColor Green(0.35f, 0.78f, 0.35f, 0.85f), Yellow(0.92f, 0.82f, 0.3f, 0.85f), Red(0.9f, 0.2f, 0.15f, 0.9f);
	return Fraction > 0.5f ? FMath::Lerp(Yellow, Green, (Fraction - 0.5f) * 2.f) : FMath::Lerp(Red, Yellow, Fraction * 2.f);
}

void UDCHUDWidget::GatherStatusIcons(const ADCCharacter* Character, TArray<FStatusIcon>& Out) const
{
	const UDCSurvivalComponent* S = Character->GetSurvivalComponent();
	const UDCHealthComponent* H = Character->GetHealthComponent();

	auto Tiered = [&Out](float Value, const float (&Limits)[4], const TCHAR* const (&Labels)[4], bool bBelow)
	{
		for (int32 L = 3; L >= 0; --L)
		{
			if (bBelow ? Value <= Limits[L] : Value >= Limits[L])
			{
				Out.Add({ Labels[L], L + 1 });
				return;
			}
		}
	};

	{ const float L[4] = { 70.f, 45.f, 20.f, 0.f };  const TCHAR* const N[4] = { TEXT("Thirsty"), TEXT("Very thirsty"), TEXT("Parched"), TEXT("Dying of thirst") }; Tiered(S->GetHydration(), L, N, true); }
	{ const float L[4] = { 70.f, 45.f, 20.f, 0.f };  const TCHAR* const N[4] = { TEXT("Peckish"), TEXT("Hungry"), TEXT("Starving"), TEXT("Wasting away") }; Tiered(S->GetEnergy(), L, N, true); }
	{ const float L[4] = { 60.f, 75.f, 90.f, 101.f }; const TCHAR* const N[4] = { TEXT("Drowsy"), TEXT("Tired"), TEXT("Exhausted"), TEXT("") }; Tiered(S->GetFatigue(), L, N, false); }
	{ const float L[4] = { 35.f, 12.f, -1.f, -2.f };  const TCHAR* const N[4] = { TEXT("Out of breath"), TEXT("Winded"), TEXT(""), TEXT("") }; Tiered(S->GetStamina(), L, N, true); }

	int32 Bleeds = 0;
	bool bFracture = false;
	for (const FDCBodyPartState& Part : H->GetBodyParts())
	{
		Bleeds += Part.bBleeding ? 1 : 0;
		bFracture |= Part.bFractured;
	}
	if (Bleeds > 0)
	{
		Out.Add({ TEXT("Bleeding"), FMath::Min(4, Bleeds + 1) });
	}
	if (bFracture)
	{
		Out.Add({ TEXT("Fracture"), 3 });
	}
	const float HealthFrac = H->GetTotalHealth() / FMath::Max(1.f, H->GetTotalMaxHealth());
	{ const float L[4] = { 0.85f, 0.65f, 0.45f, 0.25f }; const TCHAR* const N[4] = { TEXT("Hurt"), TEXT("Injured"), TEXT("Badly injured"), TEXT("Critical") }; Tiered(HealthFrac, L, N, true); }

	// Infection is never shown directly (Zomboid rules) - only its symptoms.
	if (H->IsInfected())
	{
		const float L[4] = { 0.25f, 0.5f, 0.75f, 2.f }; const TCHAR* const N[4] = { TEXT("Queasy"), TEXT("Nauseous"), TEXT("Feverish"), TEXT("") };
		Tiered(H->GetInfection(), L, N, false);
	}

	const float Weight = Character->GetInventoryComponent()->GetTotalWeight();
	const float Limit = Character->GetOverweightThresholdKg();
	if (Weight > Limit * 1.5f)      Out.Add({ TEXT("Overloaded"), 2 });
	else if (Weight > Limit)        Out.Add({ TEXT("Heavy load"), 1 });

	Out.RemoveAll([](const FStatusIcon& I) { return I.Label.IsEmpty(); });
	Out.StableSort([](const FStatusIcon& A, const FStatusIcon& B) { return A.Level > B.Level; });
}

void UDCHUDWidget::UpdateStatus(const ADCCharacter* Character, const ADCPlayerController* PC)
{
	TArray<FStatusIcon> Icons;
	GatherStatusIcons(Character, Icons);
	for (int32 i = 0; i < StatusBoxes.Num(); ++i)
	{
		if (Icons.IsValidIndex(i))
		{
			FLinearColor Color = SeverityColor(Icons[i].Level);
			if (Icons[i].Level >= 4)
			{
				Color.A *= 0.75f + 0.25f * FMath::Sin(Pulse * 7.f);
			}
			StatusBoxes[i]->SetBrushColor(Color);
			StatusTexts[i]->SetText(FText::FromString(Icons[i].Label));
			StatusTexts[i]->SetColorAndOpacity(FSlateColor(Icons[i].Level >= 3 ? FLinearColor::White : FLinearColor(0.08f, 0.06f, 0.04f)));
			StatusBoxes[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			StatusBoxes[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Stamina bar
	const UDCSurvivalComponent* S = Character->GetSurvivalComponent();
	const float Frac = FMath::Clamp(S->GetStamina() / FMath::Max(1.f, S->GetMaxStamina()), 0.f, 1.f);
	const bool bShowStamina = Frac < 0.99f;
	StaminaBack->SetVisibility(bShowStamina ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	StaminaFill->SetVisibility(bShowStamina ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(StaminaFill->Slot))
	{
		FillSlot->SetSize(FVector2D(220.f * Frac, 6.f));
	}
}

void UDCHUDWidget::UpdateBody(const ADCCharacter* Character)
{
	const UDCHealthComponent* H = Character->GetHealthComponent();
	for (const FDCBodyPartState& Part : H->GetBodyParts())
	{
		const int32 Index = static_cast<int32>(Part.Part);
		if (!BodyParts.IsValidIndex(Index))
		{
			continue;
		}
		const float Frac = Part.MaxHealth > 0.f ? Part.Health / Part.MaxHealth : 0.f;
		FLinearColor Color = Part.IsDestroyed() ? FLinearColor(0.18f, 0.05f, 0.05f, 0.9f) : HealthColor(Frac);
		if (Part.bBleeding)
		{
			Color = FMath::Lerp(Color, FLinearColor(1.f, 0.05f, 0.05f, 0.95f), 0.5f + 0.5f * FMath::Sin(Pulse * 8.f));
		}
		BodyParts[Index]->SetBrushColor(Color);
		BodyMarks[Index]->SetText(FText::FromString(Part.bFractured ? TEXT("F") : (Part.bBleeding ? TEXT("!") : TEXT(""))));
	}
	const float Total = H->GetTotalHealth() / FMath::Max(1.f, H->GetTotalMaxHealth());
	BodySummary->SetText(FText::FromString(FString::Printf(TEXT("Health %d%%"), FMath::RoundToInt(Total * 100.f))));
}

void UDCHUDWidget::UpdateWeapon(const ADCCharacter* Character)
{
	const ADCWeapon* Weapon = Character->GetEquippedWeapon();
	if (!Weapon)
	{
		WeaponNameText->SetText(FText::GetEmpty());
		AmmoText->SetText(FText::GetEmpty());
		ReserveText->SetText(FText::GetEmpty());
		return;
	}
	WeaponNameText->SetText(Weapon->GetWeaponName());
	if (Weapon->IsReloading())
	{
		AmmoText->SetText(NSLOCTEXT("DCHUD", "Reloading", "Reloading"));
	}
	else
	{
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Weapon->GetAmmoInMagazine(), Weapon->GetMagazineSize())));
		const bool bLow = Weapon->GetAmmoInMagazine() <= Weapon->GetMagazineSize() / 4;
		AmmoText->SetColorAndOpacity(FSlateColor(bLow ? SeverityColor(3) : DCUI::Dust));
	}
	ReserveText->SetText(FText::FromString(FString::Printf(TEXT("%d in reserve  [R] reload"), Weapon->GetReserveAmmo())));
}

void UDCHUDWidget::UpdateInventory(const ADCCharacter* Character, const ADCPlayerController* PC)
{
	const bool bOpen = PC && PC->IsInventoryOpen();
	InventoryPanel->SetVisibility(bOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bOpen)
	{
		return;
	}
	const UDCInventoryComponent* Inventory = Character->GetInventoryComponent();
	InventoryHeader->SetText(FText::FromString(FString::Printf(TEXT("BACKPACK  -  %.1f / %.0f kg"), Inventory->GetTotalWeight(), Character->GetOverweightThresholdKg())));

	const TArray<FDCInventoryEntry>& Entries = Inventory->GetEntries();
	for (int32 i = 0; i < InventoryRows.Num(); ++i)
	{
		UTextBlock* Row = InventoryRows[i];
		if (Entries.IsValidIndex(i) && Entries[i].Item)
		{
			const UDCItemDefinition* Item = Entries[i].Item;
			Row->SetText(FText::FromString(FString::Printf(TEXT("%s%d.  %s  x%d   %.2f kg%s"),
				i < 9 ? TEXT("") : TEXT(" "), i + 1, *Item->DisplayName.ToString(), Entries[i].Count, Item->WeightKg * Entries[i].Count,
				Item->bConsumable ? TEXT("   [use]") : TEXT(""))));
			Row->SetColorAndOpacity(FSlateColor(i < 9 ? FLinearColor::White : DCUI::Dim));
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (i == 0)
		{
			Row->SetText(NSLOCTEXT("DCHUD", "Empty", "(empty)"));
			Row->SetColorAndOpacity(FSlateColor(DCUI::Dim));
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Row->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
