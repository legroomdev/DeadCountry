#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "Net/UnrealNetwork.h"

FIntPoint FDCInventoryEntry::GetSize() const
{
	const FIntPoint Base = Item ? Item->GridSize : FIntPoint(1, 1);
	return bRotated ? FIntPoint(Base.Y, Base.X) : Base;
}

void UDCInventoryComponent::ExportSave(TArray<FDCSavedInventoryEntry>& Out) const
{
	Out.Reset();
	for (const FDCInventoryEntry& Entry : Entries)
	{
		if (Entry.Item && Entry.Count > 0)
		{
			FDCSavedInventoryEntry& Saved = Out.AddDefaulted_GetRef();
			Saved.Item = FSoftObjectPath(Entry.Item);
			Saved.Count = Entry.Count;
			Saved.Position = Entry.Position;
			Saved.bRotated = Entry.bRotated;
		}
	}
}

void UDCInventoryComponent::ImportSave(const TArray<FDCSavedInventoryEntry>& In)
{
	if (!HasAuthority())
	{
		return;
	}
	Entries.Reset();
	for (const FDCSavedInventoryEntry& Saved : In)
	{
		UDCItemDefinition* Item = Cast<UDCItemDefinition>(Saved.Item.TryLoad());
		if (!Item || Saved.Count <= 0)
		{
			continue; // item was removed from the game since this save
		}
		const FIntPoint Base = Item->GridSize;
		const FIntPoint Size = Saved.bRotated ? FIntPoint(Base.Y, Base.X) : Base;
		if (CanPlace(Saved.Position, Size))
		{
			FDCInventoryEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.EntryId = NextEntryId++;
			Entry.Item = Item;
			Entry.Count = Saved.Count;
			Entry.Position = Saved.Position;
			Entry.bRotated = Saved.bRotated;
		}
		else
		{
			AddItem(Item, Saved.Count); // grid or item size changed - find it a new spot
		}
	}
	NotifyChanged();
}

UDCInventoryComponent::UDCInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDCInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDCInventoryComponent, Entries);
}

bool UDCInventoryComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

int32 UDCInventoryComponent::AddItem(UDCItemDefinition* Item, int32 Count)
{
	if (!Item || Count <= 0 || !HasAuthority())
	{
		return Count;
	}

	int32 Remaining = Count;
	const int32 MaxStack = FMath::Max(1, Item->MaxStack);

	// Top up existing stacks first.
	for (FDCInventoryEntry& Entry : Entries)
	{
		if (Entry.Item == Item && Entry.Count < MaxStack)
		{
			const int32 Added = FMath::Min(MaxStack - Entry.Count, Remaining);
			Entry.Count += Added;
			Remaining -= Added;
			if (Remaining == 0)
			{
				break;
			}
		}
	}

	// Then open new stacks in free grid space.
	while (Remaining > 0)
	{
		FIntPoint Position;
		bool bRotated = false;
		if (!FindFreeSlot(Item->GridSize, Position, bRotated))
		{
			break;
		}

		FDCInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.EntryId = NextEntryId++;
		NewEntry.Item = Item;
		NewEntry.Count = FMath::Min(Remaining, MaxStack);
		NewEntry.Position = Position;
		NewEntry.bRotated = bRotated;
		Remaining -= NewEntry.Count;
	}

	if (Remaining != Count)
	{
		NotifyChanged();
	}
	return Remaining;
}

bool UDCInventoryComponent::RemoveFromEntry(int32 EntryId, int32 Count)
{
	if (!HasAuthority() || Count <= 0)
	{
		return false;
	}

	const int32 Index = Entries.IndexOfByPredicate([EntryId](const FDCInventoryEntry& E) { return E.EntryId == EntryId; });
	if (Index == INDEX_NONE || Entries[Index].Count < Count)
	{
		return false;
	}

	Entries[Index].Count -= Count;
	if (Entries[Index].Count <= 0)
	{
		Entries.RemoveAt(Index);
	}
	NotifyChanged();
	return true;
}

int32 UDCInventoryComponent::RemoveItem(UDCItemDefinition* Item, int32 Count)
{
	if (!HasAuthority() || !Item || Count <= 0)
	{
		return 0;
	}

	int32 Removed = 0;
	for (int32 i = Entries.Num() - 1; i >= 0 && Removed < Count; --i)
	{
		if (Entries[i].Item == Item)
		{
			const int32 Take = FMath::Min(Entries[i].Count, Count - Removed);
			Entries[i].Count -= Take;
			Removed += Take;
			if (Entries[i].Count <= 0)
			{
				Entries.RemoveAt(i);
			}
		}
	}

	if (Removed > 0)
	{
		NotifyChanged();
	}
	return Removed;
}

bool UDCInventoryComponent::TransferAllTo(UDCInventoryComponent* Target)
{
	if (!HasAuthority() || !Target || Target == this)
	{
		return IsEmpty();
	}

	bool bChanged = false;
	for (int32 i = Entries.Num() - 1; i >= 0; --i)
	{
		FDCInventoryEntry& Entry = Entries[i];
		const int32 Leftover = Target->AddItem(Entry.Item, Entry.Count);
		if (Leftover != Entry.Count)
		{
			bChanged = true;
		}
		if (Leftover <= 0)
		{
			Entries.RemoveAt(i);
		}
		else
		{
			Entry.Count = Leftover;
		}
	}

	if (bChanged)
	{
		NotifyChanged();
	}
	return IsEmpty();
}

void UDCInventoryComponent::ClearAll()
{
	if (HasAuthority() && Entries.Num() > 0)
	{
		Entries.Reset();
		NotifyChanged();
	}
}

void UDCInventoryComponent::Server_MoveEntry_Implementation(int32 EntryId, FIntPoint NewPosition, bool bNewRotated)
{
	FDCInventoryEntry* Entry = Entries.FindByPredicate([EntryId](const FDCInventoryEntry& E) { return E.EntryId == EntryId; });
	if (!Entry)
	{
		return;
	}

	const FIntPoint Base = Entry->Item ? Entry->Item->GridSize : FIntPoint(1, 1);
	const FIntPoint NewSize = bNewRotated ? FIntPoint(Base.Y, Base.X) : Base;
	if (CanPlace(NewPosition, NewSize, EntryId))
	{
		Entry->Position = NewPosition;
		Entry->bRotated = bNewRotated;
		NotifyChanged();
	}
}

int32 UDCInventoryComponent::CountItem(const UDCItemDefinition* Item) const
{
	int32 Total = 0;
	for (const FDCInventoryEntry& Entry : Entries)
	{
		if (Entry.Item == Item)
		{
			Total += Entry.Count;
		}
	}
	return Total;
}

bool UDCInventoryComponent::CanPlace(FIntPoint Position, FIntPoint Size, int32 IgnoreEntryId) const
{
	if (Position.X < 0 || Position.Y < 0 || Position.X + Size.X > GridWidth || Position.Y + Size.Y > GridHeight)
	{
		return false;
	}

	const FIntRect Candidate(Position, Position + Size);
	for (const FDCInventoryEntry& Entry : Entries)
	{
		if (Entry.EntryId == IgnoreEntryId)
		{
			continue;
		}
		const FIntRect Occupied(Entry.Position, Entry.Position + Entry.GetSize());
		const bool bOverlaps = Candidate.Min.X < Occupied.Max.X && Candidate.Max.X > Occupied.Min.X
			&& Candidate.Min.Y < Occupied.Max.Y && Candidate.Max.Y > Occupied.Min.Y;
		if (bOverlaps)
		{
			return false;
		}
	}
	return true;
}

bool UDCInventoryComponent::FindFreeSlot(FIntPoint Size, FIntPoint& OutPosition, bool& bOutRotated) const
{
	const FIntPoint Rotated(Size.Y, Size.X);
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (CanPlace(Cell, Size))
			{
				OutPosition = Cell;
				bOutRotated = false;
				return true;
			}
			if (Rotated != Size && CanPlace(Cell, Rotated))
			{
				OutPosition = Cell;
				bOutRotated = true;
				return true;
			}
		}
	}
	return false;
}

float UDCInventoryComponent::GetTotalWeight() const
{
	float Total = 0.f;
	for (const FDCInventoryEntry& Entry : Entries)
	{
		if (Entry.Item)
		{
			Total += Entry.Item->WeightKg * Entry.Count;
		}
	}
	return Total;
}

int32 UDCInventoryComponent::GetTotalValue() const
{
	int32 Total = 0;
	for (const FDCInventoryEntry& Entry : Entries)
	{
		if (Entry.Item)
		{
			Total += Entry.Item->BaseValue * Entry.Count;
		}
	}
	return Total;
}

const FDCInventoryEntry* UDCInventoryComponent::FindEntry(int32 EntryId) const
{
	return Entries.FindByPredicate([EntryId](const FDCInventoryEntry& E) { return E.EntryId == EntryId; });
}

void UDCInventoryComponent::OnRep_Entries()
{
	OnInventoryChanged.Broadcast();
}

void UDCInventoryComponent::NotifyChanged()
{
	OnInventoryChanged.Broadcast();
}
