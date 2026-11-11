#include "Player/DCPlayerController.h"
#include "Player/DCCharacter.h"
#include "Health/DCHealthComponent.h"
#include "Inventory/DCInventoryComponent.h"
#include "TimerManager.h"
#include "Survival/DCSurvivalGameMode.h"
#include "Survival/DCSurvivalGameState.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}
}

const FDCInputActions& ADCPlayerController::GetInputActions()
{
	EnsureDefaultInput();
	return Actions;
}

void ADCPlayerController::EnsureDefaultInput()
{
	if (bDefaultsBuilt)
	{
		return;
	}
	bDefaultsBuilt = true;

	if (!Actions.Move)            Actions.Move = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	if (!Actions.Look)            Actions.Look = MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	if (!Actions.Jump)            Actions.Jump = MakeAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	if (!Actions.Crouch)          Actions.Crouch = MakeAction(this, TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	if (!Actions.Sprint)          Actions.Sprint = MakeAction(this, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	if (!Actions.Interact)        Actions.Interact = MakeAction(this, TEXT("IA_Interact"), EInputActionValueType::Boolean);
	if (!Actions.Fire)            Actions.Fire = MakeAction(this, TEXT("IA_Fire"), EInputActionValueType::Boolean);
	if (!Actions.Aim)             Actions.Aim = MakeAction(this, TEXT("IA_Aim"), EInputActionValueType::Boolean);
	if (!Actions.Reload)          Actions.Reload = MakeAction(this, TEXT("IA_Reload"), EInputActionValueType::Boolean);
	if (!Actions.ToggleInventory) Actions.ToggleInventory = MakeAction(this, TEXT("IA_ToggleInventory"), EInputActionValueType::Boolean);
	if (!Actions.Sleep)           Actions.Sleep = MakeAction(this, TEXT("IA_Sleep"), EInputActionValueType::Boolean);
	if (!Actions.Respawn)         Actions.Respawn = MakeAction(this, TEXT("IA_Respawn"), EInputActionValueType::Boolean);
	if (!Actions.UseSlot)         Actions.UseSlot = MakeAction(this, TEXT("IA_UseSlot"), EInputActionValueType::Axis1D);

	if (MappingContext)
	{
		return; // A designer-provided context is in charge of key bindings.
	}

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_DCDefault"));
	UInputMappingContext* IMC = MappingContext;

	// WASD -> 2D move. W/S are swizzled onto Y; S/A are negated.
	{
		FEnhancedActionKeyMapping& W = IMC->MapKey(Actions.Move, EKeys::W);
		W.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));

		FEnhancedActionKeyMapping& S = IMC->MapKey(Actions.Move, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));

		FEnhancedActionKeyMapping& A = IMC->MapKey(Actions.Move, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));

		IMC->MapKey(Actions.Move, EKeys::D);
	}

	// Mouse look; invert Y so moving the mouse up looks up.
	{
		FEnhancedActionKeyMapping& Mouse = IMC->MapKey(Actions.Look, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(this);
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		Mouse.Modifiers.Add(NegateY);
	}

	IMC->MapKey(Actions.Jump, EKeys::SpaceBar);
	IMC->MapKey(Actions.Crouch, EKeys::C);
	IMC->MapKey(Actions.Crouch, EKeys::LeftControl);
	IMC->MapKey(Actions.Sprint, EKeys::LeftShift);
	IMC->MapKey(Actions.Interact, EKeys::F);
	IMC->MapKey(Actions.Fire, EKeys::LeftMouseButton);
	IMC->MapKey(Actions.Aim, EKeys::RightMouseButton);
	IMC->MapKey(Actions.Reload, EKeys::R);
	IMC->MapKey(Actions.ToggleInventory, EKeys::Tab);
	IMC->MapKey(Actions.Sleep, EKeys::Z);
	IMC->MapKey(Actions.Respawn, EKeys::Enter);

	// Number keys 1-9: scale the 1.0 press value by the slot number.
	const FKey SlotKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	for (int32 i = 0; i < UE_ARRAY_COUNT(SlotKeys); ++i)
	{
		FEnhancedActionKeyMapping& Slot = IMC->MapKey(Actions.UseSlot, SlotKeys[i]);
		UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(this);
		Scalar->Scalar = FVector(i + 1.0);
		Slot.Modifiers.Add(Scalar);
	}
}

void ADCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	EnsureDefaultInput();

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		Input->BindAction(Actions.ToggleInventory, ETriggerEvent::Started, this, &ADCPlayerController::Input_ToggleInventory);
		Input->BindAction(Actions.UseSlot, ETriggerEvent::Started, this, &ADCPlayerController::Input_UseSlot);
		Input->BindAction(Actions.Respawn, ETriggerEvent::Started, this, &ADCPlayerController::Input_Respawn);
	}
}

void ADCPlayerController::Input_Respawn()
{
	// Only meaningful on the death screen; the server double-checks.
	if (!GetPawn())
	{
		Server_RequestRespawn();
	}
}

void ADCPlayerController::Server_RequestRespawn_Implementation()
{
	if (ADCSurvivalGameMode* GM = GetWorld()->GetAuthGameMode<ADCSurvivalGameMode>())
	{
		GM->RespawnSurvivor(this);
	}
}

void ADCPlayerController::Client_ShowMessage_Implementation(const FText& Message)
{
	LastMessage = Message;
	LastMessageTime = GetWorld()->GetRealTimeSeconds();
}

FText ADCPlayerController::GetActiveMessage() const
{
	return (GetWorld()->GetRealTimeSeconds() - LastMessageTime) < 4.f ? LastMessage : FText::GetEmpty();
}

void ADCPlayerController::DCSuicide()
{
	ADCCharacter* Raider = GetPawn<ADCCharacter>();
	if (Raider && HasAuthority())
	{
		Raider->GetHealthComponent()->Kill(nullptr);
	}
}

void ADCPlayerController::DCShotIn(float Seconds)
{
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ConsoleCommand(TEXT("HighResShot 1"));
	}), FMath::Max(0.1f, Seconds), false);
}

void ADCPlayerController::DCSetHour(float Hour)
{
	ADCSurvivalGameState* GS = GetWorld()->GetGameState<ADCSurvivalGameState>();
	if (GS && HasAuthority())
	{
		const double DayStart = (GS->GetDay() - 1) * 24.0;
		GS->SetTotalGameHours(DayStart + FMath::Clamp(Hour, 0.f, 23.99f));
	}
}

void ADCPlayerController::Input_ToggleInventory()
{
	bInventoryOpen = !bInventoryOpen;
}

void ADCPlayerController::Input_UseSlot(const FInputActionValue& Value)
{
	if (!bInventoryOpen)
	{
		return;
	}

	ADCCharacter* Raider = GetPawn<ADCCharacter>();
	if (!Raider)
	{
		return;
	}

	const int32 SlotIndex = FMath::RoundToInt(Value.Get<float>()) - 1;
	const TArray<FDCInventoryEntry>& Entries = Raider->GetInventoryComponent()->GetEntries();
	if (Entries.IsValidIndex(SlotIndex))
	{
		Raider->Server_UseItem(Entries[SlotIndex].EntryId);
	}
}
