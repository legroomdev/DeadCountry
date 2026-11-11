#include "Weapons/DCEquippable.h"
#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "Player/DCCharacter.h"
#include "Net/UnrealNetwork.h"

ADCEquippable::ADCEquippable()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void ADCEquippable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCEquippable, EntryId);
	DOREPLIFETIME(ADCEquippable, Item);
}

void ADCEquippable::GetHeldPose(float AimAlpha, FVector& OutLocation, FRotator& OutRotation) const
{
	const float T = FMath::SmoothStep(0.f, 1.f, AimAlpha);
	OutLocation = FMath::Lerp(HipOffset, AimOffset, T);
	OutRotation = FMath::Lerp(HipRotation, FRotator::ZeroRotator, T);
}

FText ADCEquippable::GetDisplayName() const
{
	return Item ? Item->DisplayName : FText::GetEmpty();
}

void ADCEquippable::InitFromEntry(const FDCInventoryEntry& Entry)
{
	EntryId = Entry.EntryId;
	Item = Entry.Item;
	OnRep_Item();
}

void ADCEquippable::GetInstanceData(float& OutCondition, int32& OutLoadedAmmo) const
{
	OutCondition = -1.f;
	OutLoadedAmmo = -1;
}

ADCCharacter* ADCEquippable::GetOwningCharacter() const
{
	return Cast<ADCCharacter>(GetOwner());
}
