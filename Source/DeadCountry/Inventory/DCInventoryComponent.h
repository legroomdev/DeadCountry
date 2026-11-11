#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCInventoryComponent.generated.h"

class UDCItemDefinition;

USTRUCT(BlueprintType)
struct DEADCOUNTRY_API FDCInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 EntryId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDCItemDefinition> Item = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Count = 0;

	/** Top-left grid cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FIntPoint Position = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bRotated = false;

	FIntPoint GetSize() const;
};

/** Inventory entry as stored in a world save (item by asset path, so saves survive restarts). */
USTRUCT()
struct DEADCOUNTRY_API FDCSavedInventoryEntry
{
	GENERATED_BODY()

	UPROPERTY() FSoftObjectPath Item;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() FIntPoint Position = FIntPoint::ZeroValue;
	UPROPERTY() bool bRotated = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDCOnInventoryChanged);

/**
 * Replicated grid inventory. Server-authoritative: mutate only on the server,
 * clients see changes through replication and OnInventoryChanged.
 */
UCLASS(ClassGroup = (DC), meta = (BlueprintSpawnableComponent))
class DEADCOUNTRY_API UDCInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDCInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = 1))
	int32 GridWidth = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = 1))
	int32 GridHeight = 6;

	/** World save support. */
	void ExportSave(TArray<FDCSavedInventoryEntry>& Out) const;
	/** Replaces the whole inventory with saved entries (server only). */
	void ImportSave(const TArray<FDCSavedInventoryEntry>& In);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FDCOnInventoryChanged OnInventoryChanged;

	/** Adds as many as will fit. Returns the number that did NOT fit. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 AddItem(UDCItemDefinition* Item, int32 Count = 1);

	/** Removes Count from one entry; deletes the entry if it hits zero. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool RemoveFromEntry(int32 EntryId, int32 Count = 1);

	/** Removes up to Count of an item across stacks. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 RemoveItem(UDCItemDefinition* Item, int32 Count);

	/** Moves everything that fits into Target. Returns true if this inventory is now empty. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TransferAllTo(UDCInventoryComponent* Target);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void ClearAll();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory")
	void Server_MoveEntry(int32 EntryId, FIntPoint NewPosition, bool bNewRotated);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 CountItem(const UDCItemDefinition* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanPlace(FIntPoint Position, FIntPoint Size, int32 IgnoreEntryId = -1) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool FindFreeSlot(FIntPoint Size, FIntPoint& OutPosition, bool& bOutRotated) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetTotalWeight() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetTotalValue() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsEmpty() const { return Entries.Num() == 0; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FDCInventoryEntry>& GetEntries() const { return Entries; }

	const FDCInventoryEntry* FindEntry(int32 EntryId) const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FDCInventoryEntry> Entries;

	UFUNCTION()
	void OnRep_Entries();

	void NotifyChanged();
	bool HasAuthority() const;

	int32 NextEntryId = 1;
};
