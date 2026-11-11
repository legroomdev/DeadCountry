#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DCCharacter.generated.h"

class UCameraComponent;
class UDCHealthComponent;
class UDCSurvivalComponent;
class UDCInventoryComponent;
class UDCItemDefinition;
class ADCWeapon;
struct FInputActionValue;
struct FDCSavedCharacter;

USTRUCT(BlueprintType)
struct DEADCOUNTRY_API FDCStartingItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	TSoftObjectPtr<UDCItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (ClampMin = 1))
	int32 Count = 1;
};

/** The player's first-person raider. */
UCLASS()
class DEADCOUNTRY_API ADCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADCCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UDCHealthComponent* GetHealthComponent() const { return Health; }
	UDCSurvivalComponent* GetSurvivalComponent() const { return Survival; }
	UDCInventoryComponent* GetInventoryComponent() const { return Inventory; }
	ADCWeapon* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION(BlueprintPure, Category = "Character")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "Character")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Aiming")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Sleep")
	bool IsSleeping() const { return bSleeping; }

	/** World save support. Write is const; Read runs on the server after BeginPlay. */
	void WriteSaveData(FDCSavedCharacter& Out) const;
	void ReadSaveData(const FDCSavedCharacter& In);

	/** 0 = hip, 1 = fully aimed down sights (smoothed, local). */
	UFUNCTION(BlueprintPure, Category = "Aiming")
	float GetAimAlpha() const { return AimAlpha; }

	/**
	 * Kicks the local player's view (degrees, + pitch = up). Part of the pitch kick is
	 * recovered automatically over the next few frames; the rest the player has to pull down.
	 */
	void ApplyRecoil(float PitchKick, float YawKick, float RecoverFraction);

	/** Locally focused interactable (client-side only). */
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }
	FText GetFocusedInteractionText() const;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory")
	void Server_UseItem(int32 EntryId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory")
	void Server_DropItem(int32 EntryId);

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Placeholder body so other players can see you. Hidden from the owner. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDCHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDCSurvivalComponent> Survival;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDCInventoryComponent> Inventory;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<ADCWeapon> DefaultWeaponClass;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ADCWeapon> EquippedWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TArray<FDCStartingItem> StartingItems;

	UPROPERTY(ReplicatedUsing = OnRep_Sprinting)
	bool bIsSprinting = false;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float WalkSpeed = 320.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 560.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float CrouchSpeed = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float InjuredLegSpeedMultiplier = 0.6f;

	/** Carry more than this and you slow down (Zomboid-style encumbrance). */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float OverweightKg = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float OverweightSpeedMultiplier = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRange = 250.f;

	// --- Aim down sights ---
	UPROPERTY(ReplicatedUsing = OnRep_Aiming)
	bool bIsAiming = false;

	/** Weapon placement relative to the camera at the hip (weapon origin = rear of the slide). */
	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	FVector HipWeaponOffset = FVector(26.f, 10.f, -13.f);

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	FRotator HipWeaponRotation = FRotator(1.5f, -4.f, 0.f);

	/**
	 * Weapon placement when aimed. Z puts the P9's sight line (rear blade / front post tops,
	 * ~3.4cm above the weapon origin) on the camera axis; X sets the eye relief.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	FVector AimWeaponOffset = FVector(23.f, 0.f, -3.42f);

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	float HipFieldOfView = 85.f;

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	float AimFieldOfView = 68.f;

	/** Higher = snappier transition into and out of ADS. */
	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	float AimInterpSpeed = 14.f;

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	float AimMoveSpeedMultiplier = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	float AimLookSensitivity = 0.7f;

	float AimAlpha = 0.f;

	// --- Sleep ---
	UPROPERTY(ReplicatedUsing = OnRep_Sleeping)
	bool bSleeping = false;

	/** Can't fall asleep with a zombie closer than this (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Sleep")
	float SleepZombieSafetyRadius = 1500.f;

	float SleepStartTime = 0.f;
	float SleepCheckTimer = 0.f;

	/** A zombie this close wakes you up (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Sleep")
	float SleepWakeZombieRadius = 600.f;

	void UpdateSleep(float DeltaSeconds);

	void Input_ToggleSleep();

	UFUNCTION(Server, Reliable)
	void Server_ToggleSleep();

	UFUNCTION()
	void OnRep_Sleeping();

	void StartSleeping();
	void StopSleeping();
	void ApplySleepMovementState();
	bool IsZombieNearby(float Radius) const;
	void SendMessageToOwner(const FText& Message) const;

	// --- Recoil recovery (local only) ---
	/** Degrees per second the view settles back after a shot. */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float RecoilRecoverySpeed = 9.f;

	float PendingRecoilRecovery = 0.f;
	void UpdateRecoilRecovery(float DeltaSeconds);

	// Input handlers
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpStarted();
	void Input_JumpStopped();
	void Input_ToggleCrouch();
	void Input_SprintStarted();
	void Input_SprintStopped();
	void Input_Interact();
	void Input_FireStarted();
	void Input_FireStopped();
	void Input_Reload();
	void Input_AimStarted();
	void Input_AimStopped();

	void SetAiming(bool bNewAiming);
	void UpdateAimVisuals(float DeltaSeconds);

	UFUNCTION(Server, Reliable)
	void Server_SetAiming(bool bNewAiming);

	UFUNCTION()
	void OnRep_Aiming();

	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void Server_Interact(AActor* Target);

	UFUNCTION()
	void OnRep_EquippedWeapon();

	UFUNCTION()
	void OnRep_Sprinting();

	UFUNCTION()
	void HandleDeath(AController* Killer);

	UFUNCTION()
	void HandleStatusChanged();

	void SpawnDefaultWeapon();
	void GiveStartingItems();
	void AttachWeapon();
	void UpdateMovementSpeed();
	void UpdateFocus();
	bool CanSprint() const;

	TWeakObjectPtr<AActor> FocusedActor;
	float FootstepNoiseTimer = 0.f;
};
