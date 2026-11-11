#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DCZombie.generated.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;

/**
 * A shambler. Slow, tough, deaf to nothing. Headshots (real head-bone hits) do triple damage.
 * Bites can cause bleeding and (rarely) infection - Zomboid rules.
 *
 * Visuals: Epic mannequin (Manny/Quinn) with a zombie skin, single-node animation driven from C++
 * (shamble / lurch / grab), ragdoll on death. Swap meshes/anims for licensed zombie art later.
 */
UCLASS()
class DEADCOUNTRY_API ADCZombie : public ACharacter
{
	GENERATED_BODY()

public:
	ADCZombie(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Zombie")
	bool IsDead() const { return bDead; }

	/** Server: try to hit Target if in range and off cooldown. */
	bool TryAttack(AActor* Target);

	float GetAttackRange() const { return AttackRange; }

	/** Server: the AI calls this when it starts/stops chasing (drives the alert scream). */
	void SetAlerted(bool bNewAlerted);

	/** World save support. */
	float GetHealth() const { return Health; }
	void SetHealth(float NewHealth) { Health = FMath::Clamp(NewHealth, 1.f, MaxHealth); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float WalkSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float ChaseSpeed = 260.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float MaxHealth = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float HeadshotMultiplier = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float AttackDamage = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float AttackRange = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float AttackCooldown = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie", meta = (ClampMin = 0, ClampMax = 1))
	float BleedChance = 0.4f;

	/** Chance a successful bite infects. Low, but it's a death sentence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie", meta = (ClampMin = 0, ClampMax = 1))
	float InfectionChance = 0.08f;

	/** Seconds a corpse stays before being cleaned up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float CorpseLifeSeconds = 90.f;

	// --- Art (Epic mannequin placeholders) ---
	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<USkeletalMesh> MaleMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<USkeletalMesh> FemaleMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<UMaterialInterface> SkinMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TObjectPtr<UAnimSequence> JogAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	TArray<TObjectPtr<UAnimSequence>> AttackAnims;

	/** Native speeds of the walk/jog clips, used to scale play rate to movement speed (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Art")
	float WalkAnimSpeed = 165.f;

	UPROPERTY(EditDefaultsOnly, Category = "Art")
	float JogAnimSpeed = 360.f;

	// --- Replicated state ---
	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_Alerted)
	bool bAlerted = false;

	/** Direction of the killing blow, so ragdolls fall away from the shot. */
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal LastHitDirection;

	UPROPERTY(Replicated)
	FName LastHitBone;

	UFUNCTION()
	void OnRep_Dead();

	UFUNCTION()
	void OnRep_Alerted();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayAttack();

	void Die(AController* Killer);
	void UpdateAnimation();
	void UpdateAudio(float DeltaSeconds);

	float Health = 0.f;
	float LastAttackTime = -1000.f;

	// Cosmetic (every machine)
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentLoopAnim;
	float AttackAnimEndTime = -1.f;
	float NextGroanTime = 0.f;
	float ShuffleDistance = 0.f;
};
