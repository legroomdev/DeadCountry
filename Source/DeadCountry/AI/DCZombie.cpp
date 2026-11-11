#include "AI/DCZombie.h"
#include "DeadCountry.h"
#include "AI/DCZombieAIController.h"
#include "Core/DCAudio.h"
#include "Player/DCCharacter.h"
#include "Health/DCHealthComponent.h"
#include "Survival/DCSurvivalGameMode.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ADCZombie::ADCZombie(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	// Bullets pass the capsule and hit the skeletal mesh's per-bone bodies instead (real headshots).
	GetCapsuleComponent()->SetCollisionResponseToChannel(DC_TRACE_WEAPON, ECR_Ignore);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Skin(TEXT("/Game/Characters/Zombie/MI_Zombie_Skin.MI_Zombie_Skin"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Jog(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack1(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Attack2(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"));
	MaleMesh = Manny.Object;
	FemaleMesh = Quinn.Object;
	SkinMaterial = Skin.Object;
	IdleAnim = Idle.Object;
	WalkAnim = Walk.Object;
	JogAnim = Jog.Object;
	if (Attack1.Succeeded()) AttackAnims.Add(Attack1.Object);
	if (Attack2.Succeeded()) AttackAnims.Add(Attack2.Object);

	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMeshAsset(MaleMesh);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -89.f));
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f)); // mannequin faces +Y
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->SetCollisionProfileName(TEXT("CharacterMesh"));
	Body->SetCollisionResponseToChannel(DC_TRACE_WEAPON, ECR_Block);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

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
	DOREPLIFETIME(ADCZombie, bAlerted);
	DOREPLIFETIME(ADCZombie, LastHitDirection);
	DOREPLIFETIME(ADCZombie, LastHitBone);
}

void ADCZombie::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// Deterministic variety (same on every machine): roughly a third are Quinn.
	const uint32 Hash = GetTypeHash(GetFName());
	USkeletalMeshComponent* Body = GetMesh();
	if (FemaleMesh && Hash % 3 == 0)
	{
		Body->SetSkeletalMeshAsset(FemaleMesh);
	}
	if (SkinMaterial)
	{
		for (int32 i = 0; i < Body->GetNumMaterials(); ++i)
		{
			Body->SetMaterial(i, SkinMaterial);
		}
	}
	// Slightly different heights and gaits so a horde doesn't march in lockstep.
	const float Height = 0.94f + (Hash % 13) * 0.01f;
	Body->SetRelativeScale3D(FVector(Height));
	NextGroanTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 10.f);
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

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
		const FString Bone = Point.HitInfo.BoneName.ToString().ToLower();
		const bool bHeadBone = Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck"));
		const bool bHeadByHeight = Point.HitInfo.BoneName.IsNone()
			&& UDCHealthComponent::BodyPartFromCapsuleHit(this, Point.HitInfo.ImpactPoint) == EDCBodyPart::Head;
		if (bHeadBone || bHeadByHeight)
		{
			Final *= HeadshotMultiplier;
		}
		LastHitDirection = Point.ShotDirection;
		LastHitBone = Point.HitInfo.BoneName;
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
	Multicast_PlayAttack();

	// Zombies go for arms (grabbing), thorax and the neck.
	static const EDCBodyPart Targets[] = { EDCBodyPart::LeftArm, EDCBodyPart::RightArm, EDCBodyPart::Thorax, EDCBodyPart::Thorax, EDCBodyPart::Head };
	const EDCBodyPart Part = Targets[FMath::RandRange(0, UE_ARRAY_COUNT(Targets) - 1)];
	const bool bInfect = FMath::FRand() < InfectionChance;

	Victim->GetHealthComponent()->ApplyDamageToPart(Part, AttackDamage, GetController(), BleedChance, 0.f, bInfect);
	return true;
}

void ADCZombie::SetAlerted(bool bNewAlerted)
{
	if (HasAuthority() && bAlerted != bNewAlerted)
	{
		bAlerted = bNewAlerted;
		OnRep_Alerted();
	}
}

void ADCZombie::Die(AController* Killer)
{
	UE_LOG(LogDC, Log, TEXT("Zombie %s killed (bone %s)."), *GetName(), *LastHitBone.ToString());
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
	SetLifeSpan(CorpseLifeSeconds);
}

void ADCZombie::OnRep_Dead()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorTickEnabled(false);

	// Ragdoll. Physics isn't replicated - every machine simulates its own (cosmetic) fall.
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;

	// A velocity change on the struck bone (cm/s) - enough to fold them over, not launch them.
	const FVector Push = (FVector(LastHitDirection) + FVector(0.f, 0.f, 0.1f)).GetSafeNormal() * 350.f;
	const FName Bone = LastHitBone.IsNone() ? FName(TEXT("spine_03")) : LastHitBone;
	Body->AddImpulse(Push, Bone, true);
}

void ADCZombie::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Living zombies should only leave the world when the level unloads or a save is applied.
	if (EndPlayReason == EEndPlayReason::Destroyed && !bDead && HasAuthority())
	{
		UE_LOG(LogDC, Warning, TEXT("Living zombie %s removed at %s (z=%.0f)."), *GetName(), *GetActorLocation().ToCompactString(), GetActorLocation().Z);
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Animation + audio (cosmetic, every machine)
// ---------------------------------------------------------------------------

void ADCZombie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UpdateAnimation();
	UpdateAudio(DeltaSeconds);
}

void ADCZombie::UpdateAnimation()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (GetWorld()->GetTimeSeconds() < AttackAnimEndTime)
	{
		return; // let the grab play out
	}

	const float Speed = GetVelocity().Size2D();
	UAnimSequence* Want = IdleAnim;
	float Rate = 1.f;
	if (Speed > 200.f && JogAnim)
	{
		Want = JogAnim;
		Rate = FMath::Clamp(Speed / JogAnimSpeed, 0.5f, 1.3f);
	}
	else if (Speed > 12.f && WalkAnim)
	{
		Want = WalkAnim;
		// Slower than the clip's natural pace = the shamble.
		Rate = FMath::Clamp(Speed / WalkAnimSpeed, 0.35f, 1.2f);
	}

	if (Want && Want != CurrentLoopAnim)
	{
		Body->PlayAnimation(Want, true);
		CurrentLoopAnim = Want;
	}
	Body->SetPlayRate(Rate);
}

void ADCZombie::Multicast_PlayAttack_Implementation()
{
	if (bDead || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	DCAudio::PlayAt(this, EDCSound::ZombieAttack, GetActorLocation() + FVector(0, 0, 60));
	if (AttackAnims.Num() > 0)
	{
		UAnimSequence* Anim = AttackAnims[FMath::RandRange(0, AttackAnims.Num() - 1)];
		const float Rate = 1.15f;
		GetMesh()->PlayAnimation(Anim, false);
		GetMesh()->SetPlayRate(Rate);
		AttackAnimEndTime = GetWorld()->GetTimeSeconds() + Anim->GetPlayLength() / Rate;
		CurrentLoopAnim = nullptr;
	}
}

void ADCZombie::OnRep_Alerted()
{
	if (bAlerted && !bDead)
	{
		DCAudio::PlayAt(this, EDCSound::ZombieAlert, GetActorLocation() + FVector(0, 0, 60));
		NextGroanTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(2.f, 4.f);
	}
}

void ADCZombie::UpdateAudio(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextGroanTime)
	{
		DCAudio::PlayAt(this, EDCSound::ZombieGroan, GetActorLocation() + FVector(0, 0, 60), bAlerted ? 1.f : 0.7f);
		NextGroanTime = Now + (bAlerted ? FMath::FRandRange(2.5f, 5.f) : FMath::FRandRange(6.f, 14.f));
	}

	// Dragging, uneven footsteps.
	const float Speed = GetVelocity().Size2D();
	if (Speed > 20.f && GetCharacterMovement()->IsMovingOnGround())
	{
		ShuffleDistance += Speed * DeltaSeconds;
		if (ShuffleDistance >= 120.f)
		{
			ShuffleDistance = 0.f;
			const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, 85.f);
			DCAudio::PlayAt(this, EDCSound::Footstep, Feet, bAlerted ? 0.55f : 0.3f, 0.8f);
		}
	}
}
