#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DCPlayerState.generated.h"

/** Who this player is currently surviving as, and how they're doing. */
UCLASS()
class DEADCOUNTRY_API ADCPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Survivor")
	FString SurvivorName;

	/** World clock (TotalGameHours) when this survivor started. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Survivor")
	double SurvivorStartGameHours = 0.0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Survivor")
	int32 ZombieKills = 0;

	/** True between death and choosing to start a new survivor. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Survivor")
	bool bSurvivorDead = false;

	/** Days survived by the survivor who just died (for the death screen). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Survivor")
	float LastDaysSurvived = 0.f;

	UFUNCTION(BlueprintPure, Category = "Survivor")
	float GetDaysSurvived(double NowGameHours) const { return static_cast<float>((NowGameHours - SurvivorStartGameHours) / 24.0); }

	/** Stable key for this player in the world save. */
	FString GetSaveId() const;
};
