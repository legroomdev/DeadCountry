#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DCZombie.generated.h"

/**
 * A shambler. Slow, tough, deaf to nothing. Headshots do triple damage.
 * Bites can cause bleeding and (rarely) infection - Zomboid rules.
 */
UCLASS()
class DEADCOUNTRY_API ADCZombie : public ACharacter
{
	GENERATED_BODY()

public:
	ADCZombie(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Zombie")
	bool IsDead() const { return bDead; }

	/** Server: try to hit Target if in range and off cooldown. */
	bool TryAttack(AActor* Target);

	float GetAttackRange() const { return AttackRange; }

	/** World save support. */
	float GetHealth() const { return Health; }
	void SetHealth(float NewHealth) { Health = FMath::Clamp(NewHealth, 1.f, MaxHealth); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float WalkSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zombie")
	float ChaseSpeed = 260.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie")
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

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

	UPROPERTY(ReplicatedUsing = OnRep_Dead)
	bool bDead = false;

	UFUNCTION()
	void OnRep_Dead();

	void Die(AController* Killer);

	float Health = 0.f;
	float LastAttackTime = -1000.f;

	virtual void BeginPlay() override;
};
