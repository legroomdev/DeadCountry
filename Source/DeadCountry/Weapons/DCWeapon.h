#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCWeapon.generated.h"

class UStaticMeshComponent;
class UDCItemDefinition;

/**
 * Server-authoritative hitscan firearm. The owning client drives the fire cadence,
 * the server validates rate + ammo, traces, applies damage and makes noise
 * (gunshots pull every zombie in earshot - think twice before shooting).
 *
 * Visuals: the P9 is assembled from separate meshes (frame, slide, barrel, action parts,
 * magazine) that share one origin, so the action can be animated procedurally:
 * slide cycling / lock-back, barrel tilt, recoil spring compression, trigger pull, mag swap.
 */
UCLASS()
class DEADCOUNTRY_API ADCWeapon : public AActor
{
	GENERATED_BODY()

public:
	ADCWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Local input. */
	void StartFire();
	void StopFire();
	void RequestReload();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetMagazineSize() const { return MagazineSize; }

	/** World save support (server only). */
	void SetAmmoInMagazine(int32 Ammo) { if (HasAuthority()) { AmmoInMagazine = FMath::Clamp(Ammo, 0, MagazineSize); } }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return bReloading; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetReserveAmmo() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FText GetWeaponName() const { return WeaponName; }

	/** Cosmetic gun kick to add on top of the held pose. AimAlpha 0 = hip, 1 = ADS. */
	void GetVisualRecoil(float AimAlpha, FVector& OutLocation, FRotator& OutRotation) const;

protected:
	// --- Meshes (all share the weapon origin = rear-bottom of the slide) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> SlideMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> BarrelMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> RecoilSpringMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> TriggerMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMeshComponent> MagazineMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TArray<TObjectPtr<UStaticMeshComponent>> RoundMeshes;

	// --- Effects ---
	UPROPERTY(VisibleAnywhere, Category = "Effects")
	TObjectPtr<class UPointLightComponent> MuzzleLight;

	/** Two crossed emissive cards at the muzzle (front-on star + side plume). */
	UPROPERTY(VisibleAnywhere, Category = "Effects")
	TObjectPtr<UStaticMeshComponent> FlashFront;

	UPROPERTY(VisibleAnywhere, Category = "Effects")
	TObjectPtr<UStaticMeshComponent> FlashSide;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<class UMaterialInterface> BulletHoleDecal;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<class UMaterialInterface> BloodDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText WeaponName;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float Damage = 32.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float RoundsPerMinute = 450.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	bool bAutomatic = false;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 1))
	int32 MagazineSize = 17;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float RangeCm = 6000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float SpreadDegrees = 0.9f;

	/** Spread multiplier while aiming down sights. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 0, ClampMax = 1))
	float AimSpreadMultiplier = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float ReloadSeconds = 1.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float BleedChance = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float FractureChance = 0.1f;

	/** How far the shot can be heard by zombies (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float NoiseRangeCm = 6000.f;

	/** Ammo item consumed on reload. Unset or missing = infinite reserve (debug). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSoftObjectPtr<UDCItemDefinition> AmmoItem;

	UPROPERTY(Transient)
	TObjectPtr<UDCItemDefinition> ResolvedAmmoItem;

	// --- Recoil ---
	/** Upward view kick per shot (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float RecoilPitch = 1.8f;

	/** Random sideways view kick per shot (degrees, +/-), with a slight right bias. */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float RecoilYaw = 0.45f;

	/** How much of each pitch kick returns on its own (0 = none, 1 = all). */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil", meta = (ClampMin = 0, ClampMax = 1))
	float RecoilRecoverFraction = 0.75f;

	/** View recoil multiplier while aiming down sights (braced, two hands). */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil", meta = (ClampMin = 0, ClampMax = 1))
	float AimRecoilMultiplier = 0.7f;

	/** Cosmetic gun movement per shot: push back toward the shooter (cm) and muzzle flip (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float KickBackCm = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Recoil")
	float MuzzleFlipDegrees = 7.f;

	/** Muzzle flip multiplier while aiming (keeps the sights readable). */
	UPROPERTY(EditDefaultsOnly, Category = "Recoil", meta = (ClampMin = 0, ClampMax = 1))
	float AimKickMultiplier = 0.45f;

	// --- Action animation tuning (cm / degrees / seconds) ---
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float SlideTravelCm = 2.9f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float CycleSeconds = 0.09f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float BarrelTiltDegrees = 3.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float TriggerPullDegrees = 12.f;

	/** Replicated to everyone so other players see the slide lock back on an empty gun. */
	UPROPERTY(Replicated)
	int32 AmmoInMagazine = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Reloading)
	bool bReloading = false;

	UFUNCTION()
	void OnRep_Reloading();

	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector_NetQuantize ClientViewLocation, FVector_NetQuantizeNormal ClientViewDirection);

	UFUNCTION(Server, Reliable)
	void Server_Reload();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_FireEffects(FVector_NetQuantize End, FVector_NetQuantizeNormal Normal, uint8 Surface);

	/** Muzzle report + casing; played instantly for the shooter, via the multicast for everyone else. */
	void PlayShotEffects();
	void PlayImpactEffects(const FVector& End, const FVector& Normal, uint8 Surface);
	bool IsOwnerLocallyControlled() const;

	virtual void BeginPlay() override;

	void LocalFireShot();
	void FinishReload();
	float GetFireInterval() const { return 60.f / FMath::Max(1.f, RoundsPerMinute); }

	/** Most recent shot this machine knows about: the owner's own trigger pull, or the server's multicast. */
	float GetLastShotTime() const { return FMath::Max(LastFireVisualTime, LastLocalFireTime); }
	void UpdateActionVisuals();

	FTimerHandle AutoFireTimer;
	FTimerHandle ReloadTimer;
	float LastLocalFireTime = -1000.f;
	float LastServerFireTime = -1000.f;

	// Cosmetic animation state (every machine)
	float LastFireVisualTime = -1000.f;
	float ReloadStartTime = -1000.f;
	bool bTriggerHeld = false;

	// Reload sound sequencing (cosmetic)
	bool bReloadVisualActive = false;
	bool bPlayedMagOut = false;
	bool bPlayedMagIn = false;
	bool bSlideLockedAtReloadStart = false;
};
