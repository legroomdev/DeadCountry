#include "AI/DCZombie.h"
#include "AI/DCZombieAIController.h"
#include "Player/DCCharacter.h"
#include "Health/DCHealthComponent.h"
#include "Survival/DCSurvivalGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCZombie::ADCZombie(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		PlaceholderBody->SetStaticMesh(Cylinder.Object);
		PlaceholderBody->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.76f));
	}

	AIControllerClass = ADCZombieAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 240.f, 0.f);
	Move->MaxWalkSpeed = WalkSpeed;
}

void ADCZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCZombie, bDead);
}

void ADCZombie::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

float ADCZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bDead || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	float Final = DamageAmount;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		if (UDCHealthComponent::BodyPartFromCapsuleHit(this, Point.HitInfo.ImpactPoint) == EDCBodyPart::Head)
		{
			Final *= HeadshotMultiplier;
		}
	}

	Health -= Final;

	// Getting shot tells the zombie exactly where you are.
	if (ADCZombieAIController* AI = GetController<ADCZombieAIController>())
	{
		if (EventInstigator && EventInstigator->GetPawn())
		{
			AI->SetTarget(EventInstigator->GetPawn());
		}
	}

	if (Health <= 0.f)
	{
		Die(EventInstigator);
	}
	return Final;
}

bool ADCZombie::TryAttack(AActor* Target)
{
	ADCCharacter* Victim = Cast<ADCCharacter>(Target);
	if (!HasAuthority() || bDead || !Victim || Victim->IsDead())
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return false;
	}
	if (FVector::Dist(GetActorLocation(), Victim->GetActorLocation()) > AttackRange)
	{
		return false;
	}

	LastAttackTime = Now;

	// Zombies go for arms (grabbing), thorax and the neck.
	static const EDCBodyPart Targets[] = { EDCBodyPart::LeftArm, EDCBodyPart::RightArm, EDCBodyPart::Thorax, EDCBodyPart::Thorax, EDCBodyPart::Head };
	const EDCBodyPart Part = Targets[FMath::RandRange(0, UE_ARRAY_COUNT(Targets) - 1)];
	const bool bInfect = FMath::FRand() < InfectionChance;

	Victim->GetHealthComponent()->ApplyDamageToPart(Part, AttackDamage, GetController(), BleedChance, 0.f, bInfect);
	return true;
}

void ADCZombie::Die(AController* Killer)
{
	bDead = true;
	OnRep_Dead();

	if (ADCSurvivalGameMode* GM = GetWorld()->GetAuthGameMode<ADCSurvivalGameMode>())
	{
		GM->HandleZombieKilled(Killer);
	}
	if (AController* MyController = GetController())
	{
		MyController->UnPossess();
		MyController->Destroy();
	}
	SetLifeSpan(30.f);
}

void ADCZombie::OnRep_Dead()
{
	// Placeholder "ragdoll": tip over and stop blocking.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	PlaceholderBody->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	PlaceholderBody->SetRelativeLocation(FVector(0.f, 0.f, -70.f));
}
