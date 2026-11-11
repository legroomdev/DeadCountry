#include "World/DCWorldItem.h"
#include "Inventory/DCItemDefinition.h"
#include "Inventory/DCInventoryComponent.h"
#include "Player/DCCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "DCWorldItem"

ADCWorldItem::ADCWorldItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	RootComponent = Mesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetRelativeScale3D(FVector(0.2f));
	}
}

void ADCWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCWorldItem, Item);
	DOREPLIFETIME(ADCWorldItem, Count);
}

void ADCWorldItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshMesh();
}

void ADCWorldItem::OnRep_Item()
{
	RefreshMesh();
}

void ADCWorldItem::RefreshMesh()
{
	if (Item && !Item->WorldMesh.IsNull())
	{
		if (UStaticMesh* Loaded = Item->WorldMesh.LoadSynchronous())
		{
			Mesh->SetStaticMesh(Loaded);
			Mesh->SetRelativeScale3D(FVector::OneVector);
		}
	}
}

void ADCWorldItem::SetItem(UDCItemDefinition* NewItem, int32 NewCount)
{
	Item = NewItem;
	Count = FMath::Max(1, NewCount);
	RefreshMesh();
}

ADCWorldItem* ADCWorldItem::SpawnDropped(UWorld* World, UDCItemDefinition* InItem, int32 InCount, const FVector& Location, const FRotator& Rotation,
	float InCondition, int32 InLoadedAmmo)
{
	if (!World || !InItem || InCount <= 0)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADCWorldItem* Spawned = World->SpawnActor<ADCWorldItem>(ADCWorldItem::StaticClass(), Location, Rotation, Params);
	if (Spawned)
	{
		Spawned->SetItem(InItem, InCount);
		Spawned->Condition = InCondition;
		Spawned->LoadedAmmo = InLoadedAmmo;
	}
	return Spawned;
}

FText ADCWorldItem::GetInteractionText_Implementation(APawn* InstigatorPawn) const
{
	if (!Item)
	{
		return FText::GetEmpty();
	}
	if (Count > 1)
	{
		return FText::Format(LOCTEXT("PickUpMany", "Pick up {0} x{1}"), Item->DisplayName, Count);
	}
	return FText::Format(LOCTEXT("PickUp", "Pick up {0}"), Item->DisplayName);
}

bool ADCWorldItem::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return Item != nullptr && Count > 0;
}

void ADCWorldItem::Interact_Implementation(APawn* InstigatorPawn)
{
	const ADCCharacter* Character = Cast<ADCCharacter>(InstigatorPawn);
	if (!HasAuthority() || !Character || !Item)
	{
		return;
	}

	FDCInventoryEntry Picked;
	Picked.Item = Item;
	Picked.Count = Count;
	Picked.Condition = Condition;
	Picked.LoadedAmmo = LoadedAmmo;
	const int32 Leftover = Character->GetInventoryComponent()->AddEntry(Picked);
	if (Leftover <= 0)
	{
		Destroy();
	}
	else
	{
		Count = Leftover;
	}
}

#undef LOCTEXT_NAMESPACE
