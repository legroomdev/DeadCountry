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
