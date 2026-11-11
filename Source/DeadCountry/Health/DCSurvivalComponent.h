#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCSurvivalComponent.generated.h"

/**
 * Hydration, energy, fatigue and stamina. The outback heat makes water the real currency.
 * Hydration/energy/fatigue run on GAME time, so they keep pace with sleeping and time skips.
 * Server-authoritative, replicated to the owning player only.
 */
UCLASS(ClassGroup = (DC), meta = (BlueprintSpawnableComponent))
class DEADCOUNTRY_API UDCSurvivalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCSurvivalComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void AddHydration(float Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void AddEnergy(float Amount);

	/** World save support (server only). */
	void RestoreState(float InHydration, float InEnergy, float InFatigue);

	/** Server: spend stamina on a swing/shove. */
	void ConsumeStamina(float Amount) { Stamina = FMath::Max(0.f, Stamina - Amount); }

	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetHydration() const { return Hydration; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetEnergy() const { return Energy; }

	/** 0 = fully rested, 100 = falling asleep on your feet. */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetFatigue() const { return Fatigue; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetStamina() const { return Stamina; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool HasStaminaToSprint() const { return Stamina > MinStaminaToSprint; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsTired() const { return Fatigue >= TiredThreshold; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool CanSleep() const { return Fatigue >= MinFatigueToSleep; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsRested() const { return Fatigue <= RestedThreshold; }

	/** Set from the region (Red Centre > 1, Snowy Mountains < 1). */
	UPROPERTY(BlueprintReadWrite, Category = "Survival")
	float HeatStressMultiplier = 1.f;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float MaxHydration = 100.f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float MaxEnergy = 100.f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float MaxStamina = 100.f;

	/** Per in-game hour, at rest. Full to empty in ~18 game hours (more in the heat). */
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float HydrationDrainPerHour = 5.5f;
	/** Per in-game hour, at rest. Full to empty in ~28 game hours. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float EnergyDrainPerHour = 3.5f;

	/** Per in-game hour awake: ~17 hours from rested to exhausted. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float FatigueGainPerHour = 6.f;
	/** Per in-game hour asleep: a full night (~8h) clears it. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float FatigueRecoveryPerHour = 12.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float MinFatigueToSleep = 30.f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float TiredThreshold = 70.f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float RestedThreshold = 2.f;

	// Stamina runs on real time (it's moment-to-moment).
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float SprintStaminaDrainPerSecond = 12.f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float StaminaRegenPerSecond = 9.f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float MinStaminaToSprint = 5.f;

	/** Thorax/stomach damage per in-game hour while dehydrated or starving. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	float DepletedDamagePerHour = 20.f;

	UPROPERTY(Replicated)
	float Hydration = 100.f;

	UPROPERTY(Replicated)
	float Energy = 100.f;

	UPROPERTY(Replicated)
	float Fatigue = 0.f;

	UPROPERTY(Replicated)
	float Stamina = 100.f;
};
