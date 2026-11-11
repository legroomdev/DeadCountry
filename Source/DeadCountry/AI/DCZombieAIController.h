#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "DCZombieAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

UENUM()
enum class EDCZombieState : uint8
{
	Wander,
	Investigate,
	Chase
};

/**
 * Simple state machine: wander -> investigate noises -> chase what it sees.
 * Deliberately code-only for now; can move to StateTree once behaviours grow.
 */
UCLASS()
class DEADCOUNTRY_API ADCZombieAIController : public AAIController
{
	GENERATED_BODY()

public:
	ADCZombieAIController();

	virtual void Tick(float DeltaSeconds) override;

	void SetTarget(AActor* NewTarget);

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(VisibleAnywhere, Category = "AI")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float WanderRadius = 1200.f;

	/** Give up the chase after losing sight for this long. */
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float LoseTargetSeconds = 6.f;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void EnterState(EDCZombieState NewState);
	void PickWanderPoint();

	EDCZombieState State = EDCZombieState::Wander;
	TWeakObjectPtr<AActor> Target;
	FVector InvestigateLocation = FVector::ZeroVector;
	float LastSeenTargetTime = 0.f;
	float NextWanderTime = 0.f;
	float NextRepathTime = 0.f;
};
