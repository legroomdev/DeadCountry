#pragma once

#include "CoreMinimal.h"
#include "DCTypes.generated.h"

/** Tarkov-style hit zones. Head or Thorax reaching zero is fatal. */
UENUM(BlueprintType)
enum class EDCBodyPart : uint8
{
	Head,
	Thorax,
	Stomach,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDCItemCategory : uint8
{
	Misc,
	Food,
	Drink,
	Medical,
	Ammo,
	Weapon,
	Tool,
	Valuable,
	Building
};

/** The Australian environments raids can take place in. Each map picks one. */
UENUM(BlueprintType)
enum class EDCBiome : uint8
{
	RedCentre           UMETA(DisplayName = "Red Centre Outback"),
	WheatbeltTown       UMETA(DisplayName = "Wheatbelt Country Town"),
	CoastalTown         UMETA(DisplayName = "Coastal Surf Town"),
	TropicalNorth       UMETA(DisplayName = "Tropical North"),
	SnowyMountains      UMETA(DisplayName = "Snowy Mountains"),
	MiningTown          UMETA(DisplayName = "Pilbara Mining Town"),
	RegionalCity        UMETA(DisplayName = "Regional City"),
	TasmanianWilderness UMETA(DisplayName = "Tasmanian Wilderness")
};

/** Zomboid-style skills: levelled 0-10 by doing. */
UENUM(BlueprintType)
enum class EDCSkill : uint8
{
	Aiming,
	Reloading,
	ShortBlunt   UMETA(DisplayName = "Short Blunt"),
	LongBlunt    UMETA(DisplayName = "Long Blunt"),
	ShortBlade   UMETA(DisplayName = "Short Blade"),
	LongBlade    UMETA(DisplayName = "Long Blade"),
	Fitness,
	Strength,
	Sneaking,
	Maintenance,
	Carpentry,
	Cooking,
	FirstAid     UMETA(DisplayName = "First Aid"),
	MAX          UMETA(Hidden)
};

/** Sandbox option: how fast the dead move. */
UENUM(BlueprintType)
enum class EDCZombieSpeed : uint8
{
	Shamblers,
	FastShamblers UMETA(DisplayName = "Fast Shamblers"),
	Sprinters
};
