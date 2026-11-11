#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DCPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

USTRUCT(BlueprintType)
struct DEADCOUNTRY_API FDCInputActions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Move;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Look;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Jump;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Crouch;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Sprint;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Interact;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Fire;
	/** Hold to aim down sights. */
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Aim;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Reload;
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> ToggleInventory;
	/** Keys 1-9; the action value is the slot number. */
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> UseSlot;
	/** Go to sleep / wake up. */
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Sleep;
	/** On the death screen: start again as a new survivor. */
	UPROPERTY(EditAnywhere, Category = "Input") TObjectPtr<UInputAction> Respawn;
};

/**
 * Builds a default Enhanced Input setup in code so the game is playable with zero editor setup.
 * Assign your own Mapping Context / actions in a Blueprint subclass to override any of them.
 */
UCLASS()
class DEADCOUNTRY_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	const FDCInputActions& GetInputActions();

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	/** Debug console command: DCForceAim 1 holds the player fully aimed in (for tuning sight pictures). */
	UFUNCTION(Exec)
	void DCForceAim(int32 bEnable) { bDebugForceAim = bEnable != 0; }

	bool bDebugForceAim = false;

	/** Debug console command: DCSetHour 21.5 jumps the world clock to that hour today (host only). */
	UFUNCTION(Exec)
	void DCSetHour(float Hour);

	/** Debug console command: DCSuicide kills your survivor (tests death/respawn). */
	UFUNCTION(Exec)
	void DCSuicide();

	/** Debug console command: DCSpawnZombie [distance cm] spawns a zombie in front of you (host only). */
	UFUNCTION(Exec)
	void DCSpawnZombie(float Distance = 400.f);

	/** Debug console command: DCTestFire 6 [delay] fires that many shots, 0.35s apart, where you're looking. */
	UFUNCTION(Exec)
	void DCTestFire(int32 Shots = 1, float StartDelay = 0.f);

	/** Debug console command: DCShotIn 8 takes a screenshot after 8 seconds (lets exposure settle). */
	UFUNCTION(Exec)
	void DCShotIn(float Seconds);

	/** Short on-screen message from the server ("You can't sleep with zombies nearby"). */
	UFUNCTION(Client, Reliable)
	void Client_ShowMessage(const FText& Message);

	/** Current message for the HUD, empty when it has expired. */
	FText GetActiveMessage() const;

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestRespawn();

	void Input_Respawn();

	FText LastMessage;
	float LastMessageTime = -1000.f;

	int32 TestShotsRemaining = 0;
	FTimerHandle TestFireTimer;
	void TestFireOnce();

	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FDCInputActions Actions;

	void EnsureDefaultInput();
	void Input_ToggleInventory();
	void Input_UseSlot(const FInputActionValue& Value);

	bool bInventoryOpen = false;
	bool bDefaultsBuilt = false;
};
