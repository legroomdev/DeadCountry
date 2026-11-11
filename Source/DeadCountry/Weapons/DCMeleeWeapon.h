#pragma once

#include "CoreMinimal.h"
#include "Weapons/DCEquippable.h"
#include "DCMeleeWeapon.generated.h"

class UStaticMeshComponent;

/**
 * Any melee weapon - all stats come from the item definition (damage, reach, arc, swing time,
 * stamina cost, knockdown/crit chance, wear). Server-authoritative swings: the owner animates
 * instantly, the server validates, sweeps the arc at the strike moment, applies damage/knockdowns,
 * wears the weapon down and awards XP. Melee is quiet compared to gunfire.
 */
UCLASS()
class DEADCOUNTRY_API ADCMeleeWeapon : public ADCEquippable
{
	GENERATED_BODY()

public:
	ADCMeleeWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ADCEquippable
	virtual void StartPrimary() override;
	virtual bool IsBusy() const override;
	virtual void GetHeldPose(float AimAlpha, FVector& OutLocation, FRotator& OutRotation) const override;
	virtual FString GetHudPrimary() const override;
	virtual FString GetHudSecondary() const override;
	virtual float GetHudBarFraction() const override;
	virtual FQuat GetArmsReferenceRotation() const override;
	virtual void InitFromEntry(const FDCInventoryEntry& Entry) override;
	virtual void GetInstanceData(float& OutCondition, int32& OutLoadedAmmo) const override;

protected:
	virtual void OnRep_Item() override;

	UPROPERTY(VisibleAnywhere, Category = "Melee")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Replicated)
	float Condition = -1.f;

	/** Strike moment as a fraction of the swing (when the arc is swept). */
	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float StrikeFraction = 0.38f;

	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	float MinStaminaToSwing = 4.f;

	UFUNCTION(Server, Reliable)
	void Server_Swing(FVector_NetQuantize ViewLocation, FVector_NetQuantizeNormal ViewDirection);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SwingStarted(float Duration);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_Impact(FVector_NetQuantize Location, bool bBladed);

	/** Server: sweep the arc and resolve hits. */
	void PerformStrike();

	/** Swing length for the current holder (skill, exhaustion). */
	float ComputeSwingSeconds() const;

	FVector StrikeOrigin = FVector::ZeroVector;
	FVector StrikeDirection = FVector::ForwardVector;
	FTimerHandle StrikeTimer;

	// Cosmetic (every machine)
	float SwingStartTime = -1000.f;
	float SwingDuration = 0.8f;
	float ServerSwingEndTime = -1000.f;
};
