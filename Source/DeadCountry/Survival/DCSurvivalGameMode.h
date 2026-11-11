#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Survival/DCWorldSave.h"
#include "DCSurvivalGameMode.generated.h"

class UDCRegionDefinition;
class UDCWorldSave;
class ADCCharacter;

/**
 * Runs a persistent survival world (server only).
 *  - Loads the world save on start, autosaves, saves on logout and on quit.
 *  - Death is permanent for the survivor; the player respawns as a new survivor.
 *  - Speeds up time while every living survivor is asleep.
 *
 * World name comes from the URL option ?World=Name (default "Default"),
 * saved to SaveGames/DCWorld_<Name>.sav (+ _Backup).
 */
UCLASS()
class DEADCOUNTRY_API ADCSurvivalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADCSurvivalGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

	void HandleSurvivorDied(AController* Victim, AController* Killer);
	void HandleZombieKilled(AController* Killer);

	/** A dead player asked to start again as a new survivor. */
	void RespawnSurvivor(APlayerController* Player);

	/** Write the whole world to disk now. */
	UFUNCTION(Exec)
	void DCSaveWorld();

	/**
	 * This world's sandbox rules. Chosen when the world is created from URL options, e.g.
	 *   ?World=Hard?ZombieSpeed=Sprinters?Population=2?Infection=1?Loot=0.5?DayLength=45?XP=1
	 * and saved with the world (changing options later doesn't affect an existing world).
	 */
	const FDCSandboxSettings& GetSandbox() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "World")
	TObjectPtr<UDCRegionDefinition> Region;

	/** Seconds between autosaves (real time). */
	UPROPERTY(EditDefaultsOnly, Category = "World")
	float AutosaveIntervalSeconds = 300.f;

	/** Time multiplier while every living survivor is asleep. */
	UPROPERTY(EditDefaultsOnly, Category = "World")
	float SleepTimeScale = 60.f;

	/** In memory copy of the save; also holds characters of players who aren't connected. */
	UPROPERTY(Transient)
	TObjectPtr<UDCWorldSave> WorldSave;

	FString WorldName = TEXT("Default");
	FString GetSlotName() const { return TEXT("DCWorld_") + WorldName; }

	void TickWorld();
	void UpdateTimeScale();

	void LoadWorldFromDisk();
	void ParseSandboxOptions(const FString& Options);
	void ApplyStartingPopulation();
	void ApplyWorldState();
	void CaptureWorldState();
	void CaptureCharacter(APlayerController* PC);
	void WriteWorldToDisk();

	/** Restore a returning player's survivor, or set up a brand-new one. */
	void SetupSurvivor(APlayerController* PC, bool bAllowRestore);
	void StartNewSurvivor(APlayerController* PC);

	FTimerHandle WorldTickTimer;
	float SecondsSinceAutosave = 0.f;
	int32 SaveCount = 0;
	bool bWorldApplied = false;
};
