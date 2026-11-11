#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DCCharacter.generated.h"

class UCameraComponent;
class UDCHealthComponent;
class UDCSurvivalComponent;
class UDCInventoryComponent;
class UDCItemDefinition;
class ADCEquippable;
class UDCSkillsComponent;
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
	UDCSkillsComponent* GetSkillsComponent() const { return Skills; }
	ADCEquippable* GetEquippedWeapon() const { return EquippedWeapon; }

	/** Server: equip the inventory entry (or put it away if it's already in hand). */
	void EquipEntry(int32 EntryId);

	/** Server: put away whatever is in hand, writing its state (ammo, condition) back to the item. */
	void Unequip();

	/** Server: the item in hand just broke - remove it from the inventory and tell the player. */
	void HandleEquippedBroken();

	UFUNCTION(BlueprintPure, Category = "Character")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "Character")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Aiming")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Sleep")
	bool IsSleeping() const { return bSleeping; }

	/** Carry weight above which you slow down (Strength adds 2kg per level). */
	float GetOverweightThresholdKg() const;

	/** Debug: logs the first-person arm animation state and key bone positions. */
	void LogArmsDebug() const;

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

	/**
	 * First-person arms (owner only). Plays a two-handed pistol pose and is re-placed every frame so the
	 * right hand sits on the weapon's grip - the arms follow ADS, recoil and reloads automatically, and the
	 * rest of the body ends up behind the camera.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<class UAnimSequence> ArmsIdleAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<class UAnimSequence> ArmsReloadAnim;

	/** Third-person locomotion for other players' view of you. */
	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<class UAnimSequence> BodyIdleAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<class UAnimSequence> BodyWalkAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<class UAnimSequence> BodyJogAnim;

	/** Mesh-space nudge applied to the hand anchor (cm) - for lining fingers up with the grip. */
	UPROPERTY(EditDefaultsOnly, Category = "Art")
	FVector ArmsGripOffset = FVector::ZeroVector;

	/** Rotation from weapon space to arms mesh space (mannequin faces +Y). */
	UPROPERTY(EditDefaultsOnly, Category = "Art")
	FRotator ArmsRotationOffset = FRotator(0.f, -90.f, 0.f);

	void UpdateFirstPersonArms();
	void UpdateThirdPersonBody();
	FName GetHandAnchorName(const USkeletalMeshComponent* SkelMesh) const;

	UPROPERTY(Transient)
	TObjectPtr<class UAnimSequence> CurrentArmsAnim;

	UPROPERTY(Transient)
	TObjectPtr<class UAnimSequence> CurrentBodyAnim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDCHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDCSurvivalComponent> Survival;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDCInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDCSkillsComponent> Skills;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<ADCEquippable> EquippedWeapon;

	/** Item equipped when a brand-new survivor spawns. */
	UPROPERTY(EditDefaultsOnly, Category = "Loadout")
	TSoftObjectPtr<UDCItemDefinition> StartingEquippedItem;

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

	// --- Aim down sights (held poses now live on each equippable) ---
	UPROPERTY(ReplicatedUsing = OnRep_Aiming)
	bool bIsAiming = false;

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

	// --- Shove / stomp (RMB with a melee weapon or empty hands) ---
	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float ShoveStaminaCost = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float ShoveRange = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float ShoveKnockdownChance = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float StompDamage = 30.f;

	void Input_Shove();

	UFUNCTION(Server, Reliable)
	void Server_Shove();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ShoveFX(bool bStomp);

	/** Local time of the last shove/stomp (drives the shove animation). */
	float ShoveStartTime = -1000.f;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_WeaponBroke();

	/** Crouch-walking trains Sneaking; sprinting trains Fitness (server). */
	void UpdateSkillTraining(float DeltaSeconds);
	float SkillTrainingAccumulator = 0.f;

	void EquipStartingItem();
	void GiveStartingItems();
	void AttachWeapon();
	void UpdateMovementSpeed();
	void UpdateFocus();
	bool CanSprint() const;

	TWeakObjectPtr<AActor> FocusedActor;
	float FootstepNoiseTimer = 0.f;

	/** Cosmetic footstep sounds (every machine). */
	void UpdateFootstepAudio(float DeltaSeconds);
	float FootstepDistance = 0.f;
};
