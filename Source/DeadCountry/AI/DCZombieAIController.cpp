#include "AI/DCZombieAIController.h"
#include "AI/DCZombie.h"
#include "Player/DCCharacter.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

ADCZombieAIController::ADCZombieAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;

	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 1800.f;
	SightConfig->LoseSightRadius = 2300.f;
	SightConfig->PeripheralVisionAngleDegrees = 60.f;
	SightConfig->SetMaxAge(5.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 8000.f;
	HearingConfig->SetMaxAge(5.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	SetPerceptionComponent(*Perception);
}

void ADCZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ADCZombieAIController::OnTargetPerceptionUpdated);
	EnterState(EDCZombieState::Wander);
}

void ADCZombieAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		return;
	}

	const FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
	const FAISenseID HearingID = UAISense::GetSenseID<UAISense_Hearing>();

	if (Stimulus.Type == SightID)
	{
		ADCCharacter* Player = Cast<ADCCharacter>(Actor);
		if (Player && !Player->IsDead())
		{
			SetTarget(Player);
		}
	}
	else if (Stimulus.Type == HearingID && State != EDCZombieState::Chase)
	{
		InvestigateLocation = Stimulus.StimulusLocation;
		EnterState(EDCZombieState::Investigate);
	}
}

void ADCZombieAIController::SetTarget(AActor* NewTarget)
{
	Target = NewTarget;
	LastSeenTargetTime = GetWorld()->GetTimeSeconds();
	if (State != EDCZombieState::Chase)
	{
		EnterState(EDCZombieState::Chase);
	}
}

void ADCZombieAIController::EnterState(EDCZombieState NewState)
{
	State = NewState;
	ADCZombie* Zombie = GetPawn<ADCZombie>();
	if (!Zombie)
	{
		return;
	}

	Zombie->GetCharacterMovement()->MaxWalkSpeed = (NewState == EDCZombieState::Chase) ? Zombie->ChaseSpeed : Zombie->WalkSpeed;
	Zombie->SetAlerted(NewState == EDCZombieState::Chase);
	NextRepathTime = 0.f;

	switch (NewState)
	{
	case EDCZombieState::Wander:
		NextWanderTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 4.f);
		StopMovement();
		break;
	case EDCZombieState::Investigate:
		MoveToLocation(InvestigateLocation, 100.f);
		break;
	case EDCZombieState::Chase:
		break;
	}
}

void ADCZombieAIController::PickWanderPoint()
{
	const APawn* MyPawn = GetPawn();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!MyPawn || !Nav)
	{
		return;
	}

	FNavLocation Point;
	if (Nav->GetRandomReachablePointInRadius(MyPawn->GetActorLocation(), WanderRadius, Point))
	{
		MoveToLocation(Point.Location, 50.f);
	}
}

void ADCZombieAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ADCZombie* Zombie = GetPawn<ADCZombie>();
	if (!Zombie || Zombie->IsDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	switch (State)
	{
	case EDCZombieState::Wander:
		if (Now >= NextWanderTime && GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			PickWanderPoint();
			NextWanderTime = Now + FMath::FRandRange(4.f, 10.f);
		}
		break;

	case EDCZombieState::Investigate:
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			EnterState(EDCZombieState::Wander);
		}
		break;

	case EDCZombieState::Chase:
	{
		ADCCharacter* Player = Cast<ADCCharacter>(Target.Get());
		if (!Player || Player->IsDead())
		{
			Target.Reset();
			EnterState(EDCZombieState::Wander);
			break;
		}

		// Refresh "last seen" while the player is still perceived by sight.
		FActorPerceptionBlueprintInfo Info;
		if (Perception->GetActorsPerception(Player, Info))
		{
			for (const FAIStimulus& Stim : Info.LastSensedStimuli)
			{
				if (Stim.Type == UAISense::GetSenseID<UAISense_Sight>() && Stim.WasSuccessfullySensed() && !Stim.IsExpired())
				{
					LastSeenTargetTime = Now;
				}
			}
		}

		if (Now - LastSeenTargetTime > LoseTargetSeconds)
		{
			InvestigateLocation = Player->GetActorLocation();
			Target.Reset();
			EnterState(EDCZombieState::Investigate);
			break;
		}

		if (FVector::Dist(Zombie->GetActorLocation(), Player->GetActorLocation()) <= Zombie->GetAttackRange())
		{
			StopMovement();
			SetFocus(Player);
			Zombie->TryAttack(Player);
		}
		else if (Now >= NextRepathTime)
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			MoveToActor(Player, Zombie->GetAttackRange() * 0.6f);
			NextRepathTime = Now + 0.5f;
		}
		break;
	}
	}
}
