#include "World/DCLootContainer.h"
#include "Inventory/DCInventoryComponent.h"
#include "Player/DCCharacter.h"
#include "Survival/DCSurvivalGameMode.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "DCLootContainer"

ADCLootContainer::ADCLootContainer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeScale3D(FVector(0.8f, 0.5f, 0.5f));
	}

	Inventory = CreateDefaultSubobject<UDCInventoryComponent>(TEXT("Inventory"));
	Inventory->GridWidth = 6;
	Inventory->GridHeight = 4;

	ContainerName = LOCTEXT("DefaultName", "Esky");
}

void ADCLootContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCLootContainer, ContainerName);
}

void ADCLootContainer::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		RollLoot();
	}
}

void ADCLootContainer::RollLoot()
{
	// Sandbox loot rarity scales every roll.
	const ADCSurvivalGameMode* GM = GetWorld()->GetAuthGameMode<ADCSurvivalGameMode>();
	const float Rarity = GM ? GM->GetSandbox().LootRarity : 1.f;
	for (const FDCLootRoll& Roll : LootTable)
	{
		if (Roll.Item && FMath::FRand() <= Roll.Chance * Rarity)
		{
			const int32 Amount = FMath::RandRange(Roll.MinCount, FMath::Max(Roll.MinCount, Roll.MaxCount));
			Inventory->AddItem(Roll.Item, Amount);
		}
	}
}

FText ADCLootContainer::GetInteractionText_Implementation(APawn* InstigatorPawn) const
{
	if (Inventory->IsEmpty())
	{
		return FText::Format(LOCTEXT("Empty", "{0} (empty)"), ContainerName);
	}
	return FText::Format(LOCTEXT("Search", "Loot {0}"), ContainerName);
}

bool ADCLootContainer::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return !Inventory->IsEmpty();
}

void ADCLootContainer::Interact_Implementation(APawn* InstigatorPawn)
{
	const ADCCharacter* Character = Cast<ADCCharacter>(InstigatorPawn);
	if (!HasAuthority() || !Character)
	{
		return;
	}

	const bool bNowEmpty = Inventory->TransferAllTo(Character->GetInventoryComponent());
	if (bNowEmpty && bDestroyWhenEmpty)
	{
		SetLifeSpan(1.f);
	}
}

#undef LOCTEXT_NAMESPACE
