#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DCTypes.h"
#include "DCHealthComponent.generated.h"

USTRUCT(BlueprintType)
struct DEADCOUNTRY_API FDCBodyPartState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	EDCBodyPart Part = EDCBodyPart::Thorax;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	bool bBleeding = false;

	UPROPERTY(BlueprintReadOnly, Category = "Health")
	bool bFractured = false;

	bool IsDestroyed() const { return Health <= 0.f; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDCOnDeath, AController*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnHealthChanged);

/**
 * Per-limb health (Tarkov) + bleeding, fractures and zombie infection (Zomboid).
 * Server-authoritative; replicated to everyone so other players can see you limping.
 */
UCLASS(ClassGroup = (DC), meta = (BlueprintSpawnableComponent))
class DEADCOUNTRY_API UDCHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FDCOnDeath OnDeath;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FDCOnHealthChanged OnHealthChanged;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void ApplyDamageToPart(EDCBodyPart Part, float Amount, AController* DamageInstigator, float BleedChance = 0.f, float FractureChance = 0.f, bool bInfect = false);

	/** Heals the most damaged, non-destroyed parts first. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void Heal(float Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	bool StopOneBleed();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	bool SplintOneFracture();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Health")
	void Kill(AController* Killer);

	/** World save support (server only). */
	void RestoreState(const TArray<FDCBodyPartState>& Parts, float InInfection, bool bInInfected);

	/** World time of the last damage dealt by someone/something (not bleeding/thirst). Wakes sleepers. */
	float GetLastAttackedTime() const { return LastAttackedTime; }

	UFUNCTION(BlueprintPure, Category = "Health")
	const TArray<FDCBodyPartState>& GetBodyParts() const { return BodyParts; }

	UFUNCTION(BlueprintPure, Category = "Health")
	FDCBodyPartState GetPart(EDCBodyPart Part) const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetTotalHealth() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetTotalMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsBleeding() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	bool HasLegInjury() const;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetInfection() const { return Infection; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsInfected() const { return bInfected; }

	/** Maps a hit point on a capsule to a body part (placeholder until we have a skeleton). */
	static EDCBodyPart BodyPartFromCapsuleHit(const AActor* Victim, const FVector& WorldHitLocation);

	static FText GetBodyPartName(EDCBodyPart Part);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float HeadMax = 35.f;
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float ThoraxMax = 85.f;
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float StomachMax = 70.f;
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float ArmMax = 60.f;
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float LegMax = 65.f;

	/** Damage per second per bleeding limb. */
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float BleedDamagePerSecond = 0.8f;

	/** Damage to a destroyed limb is spread to the rest of the body at this multiplier. */
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float DestroyedLimbSpreadMultiplier = 0.7f;

	/** Seconds from bite to turning. Zomboid's knox infection is a death sentence. */
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float InfectionSecondsToTurn = 900.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	TArray<FDCBodyPartState> BodyParts;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Infection = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	bool bInfected = false;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	bool bDead = false;

	UFUNCTION()
	void OnRep_Health();

	void InitBodyParts();
	FDCBodyPartState* FindPart(EDCBodyPart Part);
	void CheckDeath(AController* DamageInstigator);
	bool HasAuthority() const;

	float LastAttackedTime = -1000.f;
};
