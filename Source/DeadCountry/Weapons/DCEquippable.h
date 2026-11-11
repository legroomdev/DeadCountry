#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCEquippable.generated.h"

class UDCItemDefinition;
class ADCCharacter;
struct FDCInventoryEntry;

/**
 * Anything a survivor holds in their hands (firearms, melee weapons, later tools).
 * Spawned by the server when an inventory item is equipped; carries that item's per-instance
 * state (condition, loaded ammo) and writes it back when unequipped.
 *
 * The character asks it for a camera-relative held pose every frame (hip/ADS, recoil, swings),
 * and the first-person arms lock onto GetGripTransform().
 */
UCLASS(Abstract)
class DEADCOUNTRY_API ADCEquippable : public AActor
{
	GENERATED_BODY()

public:
	ADCEquippable();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Input (called on the owning client) ---
	virtual void StartPrimary() {}
	virtual void StopPrimary() {}
	virtual void RequestReload() {}

	/** True if the secondary button aims down sights (firearms); otherwise it shoves. */
	virtual bool CanAim() const { return false; }

	/** Busy with a swing / reload (blocks swapping and shoving). */
	virtual bool IsBusy() const { return false; }

	// --- Presentation ---
	/** Camera-relative transform for this frame. AimAlpha 0 = hip, 1 = fully aimed. */
	virtual void GetHeldPose(float AimAlpha, FVector& OutLocation, FRotator& OutRotation) const;

	/** World transform of the dominant hand's grip. */
	virtual FTransform GetGripTransform() const { return GetActorTransform(); }

	/** Grip relative to the actor origin. */
	virtual FTransform GetGripLocalTransform() const { return FTransform::Identity; }

	/**
	 * Orientation the first-person arms are built around. Guns: the gun itself. Melee weapons point every
	 * which way mid-swing, so they use the view direction and only the hand position follows the weapon.
	 */
	virtual FQuat GetArmsReferenceRotation() const { return GetActorQuat(); }

	/** Two-handed pistol arm pose is used for everything for now; reload clip only for firearms. */
	virtual bool WantsReloadArmsPose() const { return false; }
	virtual float GetReloadDuration() const { return 1.f; }

	virtual FText GetDisplayName() const;
	/** Big HUD line ("12 / 17", "Condition 84%"). */
	virtual FString GetHudPrimary() const { return FString(); }
	/** Small HUD line ("34 in reserve"). */
	virtual FString GetHudSecondary() const { return FString(); }
	/** 0..1 for an optional HUD bar (condition), < 0 for none. */
	virtual float GetHudBarFraction() const { return -1.f; }

	// --- Item instance ---
	/** Server: set up from the inventory entry it represents. */
	virtual void InitFromEntry(const FDCInventoryEntry& Entry);

	/** Current per-instance state to write back to the inventory entry. */
	virtual void GetInstanceData(float& OutCondition, int32& OutLoadedAmmo) const;

	int32 GetEntryId() const { return EntryId; }
	UDCItemDefinition* GetItem() const { return Item; }
	ADCCharacter* GetOwningCharacter() const;

protected:
	UPROPERTY(Replicated)
	int32 EntryId = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Item)
	TObjectPtr<UDCItemDefinition> Item;

	UFUNCTION()
	virtual void OnRep_Item() {}

	/** Default held pose: hip and aimed transforms relative to the camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Held")
	FVector HipOffset = FVector(27.f, 9.5f, -11.f);

	UPROPERTY(EditDefaultsOnly, Category = "Held")
	FRotator HipRotation = FRotator(1.5f, -4.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Held")
	FVector AimOffset = FVector(23.f, 0.f, -3.42f);
};
