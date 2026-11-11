#include "Weapons/DCShellCasing.h"
#include "DeadCountry.h"
#include "Core/DCAudio.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

TArray<TWeakObjectPtr<ADCShellCasing>> ADCShellCasing::LiveCasings;

ADCShellCasing::ADCShellCasing()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	InitialLifeSpan = 20.f;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Casing(TEXT("/Game/Weapons/P9/SM_P9_Casing.SM_P9_Casing"));
	if (Casing.Succeeded())
	{
		Mesh->SetStaticMesh(Casing.Object);
	}
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore); // casings don't pile-up-collide
	Mesh->SetCollisionResponseToChannel(DC_TRACE_WEAPON, ECR_Ignore);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->BodyInstance.bUseCCD = true;
	Mesh->SetLinearDamping(0.3f);
	Mesh->SetAngularDamping(0.6f);
	Mesh->CastShadow = false;
	Mesh->SetGenerateOverlapEvents(false);
}

void ADCShellCasing::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetMassOverrideInKg(NAME_None, 0.05f, true);
	Mesh->OnComponentHit.AddDynamic(this, &ADCShellCasing::OnCasingHit);

	// Keep the world tidy: oldest casing goes when the cap is hit.
	LiveCasings.RemoveAll([](const TWeakObjectPtr<ADCShellCasing>& C) { return !C.IsValid(); });
	if (LiveCasings.Num() >= MaxLiveCasings)
	{
		if (ADCShellCasing* Oldest = LiveCasings[0].Get())
		{
			Oldest->Destroy();
		}
		LiveCasings.RemoveAt(0);
	}
	LiveCasings.Add(this);
}

void ADCShellCasing::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LiveCasings.Remove(this);
	Super::EndPlay(EndPlayReason);
}

void ADCShellCasing::Eject(UWorld* World, const FTransform& PortTransform, const FVector& InheritedVelocity)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Casing mesh lies along +X; turn it sideways so it leaves the port broadside.
	const FRotator SpawnRot = PortTransform.Rotator() + FRotator(0.f, 90.f, 0.f);
	ADCShellCasing* Casing = World->SpawnActor<ADCShellCasing>(ADCShellCasing::StaticClass(), PortTransform.GetLocation(), SpawnRot, Params);
	if (!Casing)
	{
		return;
	}

	const FQuat Q = PortTransform.GetRotation();
	const FVector Right = Q.GetRightVector();
	const FVector Up = Q.GetUpVector();
	const FVector Back = -Q.GetForwardVector();
	const FVector Velocity = Right * FMath::FRandRange(220.f, 320.f)
		+ Up * FMath::FRandRange(120.f, 200.f)
		+ Back * FMath::FRandRange(20.f, 70.f)
		+ InheritedVelocity;
	Casing->Mesh->SetPhysicsLinearVelocity(Velocity);
	Casing->Mesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(600.f, 1400.f), FMath::FRandRange(-300.f, 300.f)));
}

void ADCShellCasing::OnCasingHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed = Mesh->GetPhysicsLinearVelocity().Size();
	if (TinkCount >= 3 || Now - LastTinkTime < 0.12f || Speed < 25.f)
	{
		return;
	}
	LastTinkTime = Now;
	++TinkCount;
	const float Volume = FMath::Clamp(Speed / 300.f, 0.15f, 0.8f) * (TinkCount == 1 ? 1.f : 0.6f);
	DCAudio::PlayAt(this, EDCSound::CasingTink, GetActorLocation(), Volume);
}
