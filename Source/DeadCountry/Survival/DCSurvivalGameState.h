#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DCSurvivalGameState.generated.h"

class ADirectionalLight;
class UDCRegionDefinition;

/**
 * The world clock. TotalGameHours counts in-game hours since the world was created
 * (Day 1 00:00 = 0). The server owns it; clients extrapolate between updates.
 * Also drives the sun and moon for day/night.
 */
UCLASS()
class DEADCOUNTRY_API ADCSurvivalGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADCSurvivalGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Time")
	double GetTotalGameHours() const { return TotalGameHours; }

	/** 1-based day number. */
	UFUNCTION(BlueprintPure, Category = "Time")
	int32 GetDay() const { return FMath::FloorToInt(TotalGameHours / 24.0) + 1; }

	UFUNCTION(BlueprintPure, Category = "Time")
	float GetHourOfDay() const { return static_cast<float>(FMath::Fmod(TotalGameHours, 24.0)); }

	UFUNCTION(BlueprintPure, Category = "Time")
	bool IsNight() const { const float H = GetHourOfDay(); return H < 6.f || H >= 19.f; }

	/** In-game hours that pass per real second, including any time skip (sleeping). */
	UFUNCTION(BlueprintPure, Category = "Time")
	float GetGameHoursPerSecond() const;

	UFUNCTION(BlueprintPure, Category = "Time")
	float GetTimeScale() const { return TimeScale; }

	/** "Day 3  14:05" */
	FString GetClockString() const;

	void SetTotalGameHours(double Hours);
	void SetTimeScale(float NewScale);

	/** Real-time minutes for one in-game day. Zomboid's default is one hour. */
	UPROPERTY(EditDefaultsOnly, Category = "Time", meta = (ClampMin = 1))
	float DayLengthMinutes = 60.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "World")
	TObjectPtr<UDCRegionDefinition> Region;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Clock)
	double ReplicatedGameHours = 8.0;

	UPROPERTY(Replicated)
	float TimeScale = 1.f;

	UFUNCTION()
	void OnRep_Clock();

	void FindSkyLights();
	void UpdateSky();

	/** Locally advanced clock (smooth between replication updates). */
	double TotalGameHours = 8.0;
	float ReplicateAccumulator = 0.f;

	TWeakObjectPtr<ADirectionalLight> Sun;
	TWeakObjectPtr<ADirectionalLight> Moon;
	TWeakObjectPtr<class AExponentialHeightFog> Fog;
	FLinearColor FogDayColor = FLinearColor::Black;
};
