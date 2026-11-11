#include "UI/DCHUD.h"
#include "Player/DCCharacter.h"
#include "Player/DCPlayerController.h"
#include "Health/DCHealthComponent.h"
#include "Health/DCSurvivalComponent.h"
#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "Weapons/DCWeapon.h"
#include "Survival/DCPlayerState.h"
#include "Survival/DCSurvivalGameState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

namespace DCHUDColors
{
	const FLinearColor Dust(0.93f, 0.78f, 0.55f);
	const FLinearColor Warn(1.f, 0.55f, 0.15f);
	const FLinearColor Bad(1.f, 0.2f, 0.15f);
	const FLinearColor Good(0.45f, 1.f, 0.5f);
	const FLinearColor Dim(0.7f, 0.7f, 0.7f);
}

void ADCHUD::DrawLine(const FString& Text, float X, float& Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	DrawText(Text, Color, X, Y, Font, Scale);
	Y += 20.f * Scale;
}

void ADCHUD::DrawCrosshair()
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	DrawRect(FLinearColor(1, 1, 1, 0.8f), CX - 1.f, CY - 1.f, 3.f, 3.f);
}

void ADCHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayerController());
	const ADCPlayerState* PS = PC ? PC->GetPlayerState<ADCPlayerState>() : nullptr;
	const ADCSurvivalGameState* GS = GetWorld()->GetGameState<ADCSurvivalGameState>();
	ADCCharacter* Character = PC ? PC->GetPawn<ADCCharacter>() : nullptr;

	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;

	// --- Death screen ---
	if (PS && PS->bSurvivorDead && !Character)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), 0.f, 0.f, W, H);
		float Y = H * 0.38f;
		const float X = W * 0.5f - 160.f;
		DrawLine(TEXT("YOU DIED"), X, Y, DCHUDColors::Bad, 2.5f);
		DrawLine(FString::Printf(TEXT("%s survived %.1f days and killed %d zombies."),
			*PS->SurvivorName, PS->LastDaysSurvived, PS->ZombieKills), X, Y, DCHUDColors::Dust);
		DrawLine(TEXT("Their body is still out there, with everything they carried."), X, Y, DCHUDColors::Dim);
		Y += 20.f;
		DrawLine(TEXT("Press ENTER to start again as a new survivor"), X, Y, FLinearColor::White, 1.2f);
		return;
	}

	// --- Sleep overlay ---
	if (Character && Character->IsSleeping())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.02f, 0.93f), 0.f, 0.f, W, H);
		float Y = H * 0.45f;
		const float X = W * 0.5f - 110.f;
		DrawLine(TEXT("Sleeping..."), X, Y, DCHUDColors::Dim, 1.6f);
		if (GS)
		{
			DrawLine(GS->GetClockString(), X, Y, DCHUDColors::Dust, 1.3f);
		}
		if (const UDCSurvivalComponent* Survival = Character->GetSurvivalComponent())
		{
			DrawLine(FString::Printf(TEXT("Fatigue %.0f"), Survival->GetFatigue()), X, Y, DCHUDColors::Dim);
		}
		DrawLine(TEXT("[Z] wake up"), X, Y, DCHUDColors::Dim);
		return;
	}

	// --- Clock + survivor (top centre) ---
	{
		float Y = 18.f;
		if (GS)
		{
			DrawLine(GS->GetClockString(), W * 0.5f - 60.f, Y, GS->IsNight() ? FLinearColor(0.6f, 0.7f, 1.f) : DCHUDColors::Dust, 1.3f);
		}
		if (PS && GS && !PS->SurvivorName.IsEmpty())
		{
			DrawLine(FString::Printf(TEXT("%s  -  %.1f days survived"), *PS->SurvivorName, PS->GetDaysSurvived(GS->GetTotalGameHours())),
				W * 0.5f - 110.f, Y, DCHUDColors::Dim);
		}
	}

	// --- Server messages ---
	if (PC)
	{
		const FText Message = PC->GetActiveMessage();
		if (!Message.IsEmpty())
		{
			float Y = H * 0.32f;
			DrawLine(Message.ToString(), W * 0.5f - 150.f, Y, FLinearColor::White, 1.2f);
		}
	}

	if (!Character)
	{
		return;
	}

	// The iron sights are the reticle once you're mostly aimed in.
	if (Character->GetAimAlpha() < 0.5f)
	{
		DrawCrosshair();
	}

	// --- Interaction prompt ---
	const FText Prompt = Character->GetFocusedInteractionText();
	if (!Prompt.IsEmpty())
	{
		float Y = H * 0.5f + 30.f;
		DrawLine(FString::Printf(TEXT("[F] %s"), *Prompt.ToString()), W * 0.5f - 80.f, Y, DCHUDColors::Dust);
	}

	// --- Body status (bottom left) ---
	if (const UDCHealthComponent* Health = Character->GetHealthComponent())
	{
		float Y = H - 250.f;
		for (const FDCBodyPartState& Part : Health->GetBodyParts())
		{
			FString Line = FString::Printf(TEXT("%-8s %3.0f/%.0f"), *UDCHealthComponent::GetBodyPartName(Part.Part).ToString(), Part.Health, Part.MaxHealth);
			if (Part.bBleeding)  Line += TEXT("  BLEEDING");
			if (Part.bFractured) Line += TEXT("  FRACTURE");
			const FLinearColor Color = Part.IsDestroyed() ? DCHUDColors::Bad : (Part.bBleeding || Part.bFractured ? DCHUDColors::Warn : DCHUDColors::Dust);
			DrawLine(Line, 20.f, Y, Color);
		}
		if (Health->IsInfected())
		{
			DrawLine(FString::Printf(TEXT("INFECTED %d%%"), FMath::RoundToInt(Health->GetInfection() * 100.f)), 20.f, Y, DCHUDColors::Bad);
		}
	}

	if (const UDCSurvivalComponent* Survival = Character->GetSurvivalComponent())
	{
		float Y = H - 90.f;
		DrawLine(FString::Printf(TEXT("Water %3.0f   Energy %3.0f   Stamina %3.0f"), Survival->GetHydration(), Survival->GetEnergy(), Survival->GetStamina()),
			20.f, Y, Survival->GetHydration() < 20.f ? DCHUDColors::Warn : DCHUDColors::Dim);
		const FString Tired = Survival->IsTired() ? TEXT("  TIRED") : (Survival->CanSleep() ? TEXT("  [Z] sleep") : TEXT(""));
		DrawLine(FString::Printf(TEXT("Fatigue %3.0f%s"), Survival->GetFatigue(), *Tired), 20.f, Y,
			Survival->IsTired() ? DCHUDColors::Warn : DCHUDColors::Dim);
	}

	// --- Weapon (bottom right) ---
	if (const ADCWeapon* Weapon = Character->GetEquippedWeapon())
	{
		float Y = H - 70.f;
		const FString Ammo = Weapon->IsReloading()
			? TEXT("Reloading...")
			: FString::Printf(TEXT("%d / %d  [%d]"), Weapon->GetAmmoInMagazine(), Weapon->GetMagazineSize(), Weapon->GetReserveAmmo());
		DrawLine(Weapon->GetWeaponName().ToString(), W - 260.f, Y, DCHUDColors::Dim);
		DrawLine(Ammo, W - 260.f, Y, DCHUDColors::Dust, 1.2f);
	}

	// --- Inventory (Tab) ---
	if (PC && PC->IsInventoryOpen())
	{
		const UDCInventoryComponent* Inventory = Character->GetInventoryComponent();
		const float X = W * 0.5f + 80.f;
		float Y = 120.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 15.f, Y - 15.f, 460.f, 60.f + 20.f * FMath::Max(1, Inventory->GetEntries().Num()) + 40.f);
		DrawLine(FString::Printf(TEXT("BACKPACK  %.1f kg"), Inventory->GetTotalWeight()), X, Y, DCHUDColors::Dust, 1.2f);
		DrawLine(TEXT("Press 1-9 to use an item"), X, Y, DCHUDColors::Dim);

		int32 Index = 1;
		for (const FDCInventoryEntry& Entry : Inventory->GetEntries())
		{
			const UDCItemDefinition* Item = Entry.Item;
			if (!Item)
			{
				continue;
			}
			const FString Line = FString::Printf(TEXT("%d. %s x%d  (%dx%d, %.2fkg)%s"),
				Index, *Item->DisplayName.ToString(), Entry.Count, Entry.GetSize().X, Entry.GetSize().Y, Item->WeightKg * Entry.Count,
				Item->bConsumable ? TEXT("  [use]") : TEXT(""));
			DrawLine(Line, X, Y, Index <= 9 ? FLinearColor::White : DCHUDColors::Dim);
			++Index;
		}
		if (Inventory->GetEntries().Num() == 0)
		{
			DrawLine(TEXT("(empty)"), X, Y, DCHUDColors::Dim);
		}
	}
}
