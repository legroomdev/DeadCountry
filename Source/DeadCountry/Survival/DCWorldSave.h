#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Health/DCHealthComponent.h"
#include "Inventory/DCInventoryComponent.h"
#include "DCWorldSave.generated.h"

/** Bump when the layout changes; LoadWorld refuses saves from a newer version. */
static constexpr int32 DC_WORLD_SAVE_VERSION = 2; // 2: item instances, skills, equipped item, sandbox

/** Rules chosen when a world is created (Zomboid-style sandbox options). */
USTRUCT(BlueprintType)
struct FDCSandboxSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandbox") EDCZombieSpeed ZombieSpeed = EDCZombieSpeed::Shamblers;
	/** Multiplier on the zombies a new world starts with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandbox") float ZombiePopulation = 1.f;
	/** Multiplier on a bite's chance to infect (0 = no infection). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandbox") float InfectionChanceScale = 1.f;
	/** Multiplier on every loot roll's chance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandbox") float LootRarity = 1.f;
	/** Real minutes per in-game day. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandbox") float DayLengthMinutes = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sandbox") float XPMultiplier = 1.f;
};

USTRUCT()
struct FDCSavedContainer
{
	GENERATED_BODY()

	/** Level-placed containers are matched by actor name; spawned ones (bodies) are recreated. */
	UPROPERTY() FName ActorName;
	UPROPERTY() bool bSpawned = false;
	UPROPERTY() FTransform Transform;
	UPROPERTY() FText DisplayName;
	UPROPERTY() bool bDestroyWhenEmpty = false;
	UPROPERTY() int32 GridWidth = 6;
	UPROPERTY() int32 GridHeight = 4;
	UPROPERTY() TArray<FDCSavedInventoryEntry> Entries;
};

USTRUCT()
struct FDCSavedWorldItem
{
	GENERATED_BODY()

	UPROPERTY() FSoftObjectPath Item;
	UPROPERTY() int32 Count = 1;
	UPROPERTY() FTransform Transform;
	UPROPERTY() float Condition = -1.f;
	UPROPERTY() int32 LoadedAmmo = -1;
};

USTRUCT()
struct FDCSavedZombie
{
	GENERATED_BODY()

	UPROPERTY() FTransform Transform;
	UPROPERTY() float Health = 0.f;
};

USTRUCT()
struct FDCSavedCharacter
{
	GENERATED_BODY()

	UPROPERTY() FString SurvivorName;
	UPROPERTY() double StartGameHours = 0.0;
	UPROPERTY() int32 ZombieKills = 0;

	UPROPERTY() FTransform Transform;
	UPROPERTY() FRotator ControlRotation = FRotator::ZeroRotator;

	UPROPERTY() TArray<FDCBodyPartState> BodyParts;
	UPROPERTY() float Infection = 0.f;
	UPROPERTY() bool bInfected = false;

	UPROPERTY() float Hydration = 100.f;
	UPROPERTY() float Energy = 100.f;
	UPROPERTY() float Fatigue = 0.f;

	UPROPERTY() TArray<FDCSavedInventoryEntry> Inventory;
	UPROPERTY() int32 WeaponAmmo = -1; // v1 only

	/** Grid cell of the item in hand (its entry is matched by position after loading). */
	UPROPERTY() bool bHasEquipped = false;
	UPROPERTY() FIntPoint EquippedPosition = FIntPoint::ZeroValue;

	UPROPERTY() TArray<float> SkillXP;
};

/** Everything that makes a world persistent. One save slot per world. */
UCLASS()
class DEADCOUNTRY_API UDCWorldSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 SaveVersion = DC_WORLD_SAVE_VERSION;
	UPROPERTY() FString WorldName;
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() double GameHours = 0.0;
	UPROPERTY() FDCSandboxSettings Sandbox;

	UPROPERTY() TArray<FDCSavedContainer> Containers;
	UPROPERTY() TArray<FDCSavedWorldItem> WorldItems;
	UPROPERTY() TArray<FDCSavedZombie> Zombies;

	/** Living survivors keyed by player save id (see ADCPlayerState::GetSaveId). */
	UPROPERTY() TMap<FString, FDCSavedCharacter> Characters;
};
