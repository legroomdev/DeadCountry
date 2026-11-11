#include "Survival/DCSurvivalGameMode.h"
#include "DeadCountry.h"
#include "Survival/DCPlayerState.h"
#include "Survival/DCSurvivalGameState.h"
#include "Survival/DCWorldSave.h"
#include "World/DCRegionDefinition.h"
#include "World/DCLootContainer.h"
#include "World/DCWorldItem.h"
#include "AI/DCZombie.h"
#include "Player/DCCharacter.h"
#include "Player/DCPlayerController.h"
#include "Health/DCSurvivalComponent.h"
#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "UI/DCHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace DCSurvivorNames
{
	const TCHAR* First[] = {
		TEXT("Shane"), TEXT("Kylie"), TEXT("Darren"), TEXT("Tash"), TEXT("Bruce"), TEXT("Sheila"), TEXT("Wayne"),
		TEXT("Shazza"), TEXT("Mick"), TEXT("Bec"), TEXT("Trev"), TEXT("Jodie"), TEXT("Gaz"), TEXT("Narelle"),
		TEXT("Craig"), TEXT("Leanne"), TEXT("Robbo"), TEXT("Kirsty"), TEXT("Dazza"), TEXT("Michelle") };
	const TCHAR* Last[] = {
		TEXT("Kelly"), TEXT("Nguyen"), TEXT("Smith"), TEXT("O'Brien"), TEXT("Papadopoulos"), TEXT("Wilson"),
		TEXT("Morgan"), TEXT("Rossi"), TEXT("Campbell"), TEXT("Taylor"), TEXT("Walker"), TEXT("Brown"),
		TEXT("Murphy"), TEXT("Singh"), TEXT("Jones"), TEXT("Harris") };

	FString Make()
	{
		return FString::Printf(TEXT("%s %s"),
			First[FMath::RandRange(0, UE_ARRAY_COUNT(First) - 1)],
			Last[FMath::RandRange(0, UE_ARRAY_COUNT(Last) - 1)]);
	}
}

ADCSurvivalGameMode::ADCSurvivalGameMode()
{
	DefaultPawnClass = ADCCharacter::StaticClass();
	PlayerControllerClass = ADCPlayerController::StaticClass();
	PlayerStateClass = ADCPlayerState::StaticClass();
	GameStateClass = ADCSurvivalGameState::StaticClass();
	HUDClass = ADCHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ADCSurvivalGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	const FString OptionWorld = UGameplayStatics::ParseOption(Options, TEXT("World"));
	if (!OptionWorld.IsEmpty())
	{
		WorldName = OptionWorld;
	}
	LoadWorldFromDisk();
}

void ADCSurvivalGameMode::StartPlay()
{
	Super::StartPlay(); // actors' BeginPlay runs here (containers roll fresh loot, players get kits)

	if (ADCSurvivalGameState* GS = GetGameState<ADCSurvivalGameState>())
	{
		GS->Region = Region;
	}

	ApplyWorldState();
	bWorldApplied = true;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			SetupSurvivor(PC, true);
		}
	}

	GetWorldTimerManager().SetTimer(WorldTickTimer, this, &ADCSurvivalGameMode::TickWorld, 1.f, true);
}

void ADCSurvivalGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Players present at startup are set up in StartPlay, once the world save has been applied.
	if (bWorldApplied)
	{
		SetupSurvivor(NewPlayer, true);
	}
}

void ADCSurvivalGameMode::Logout(AController* Exiting)
{
	if (bWorldApplied)
	{
		if (APlayerController* PC = Cast<APlayerController>(Exiting))
		{
			CaptureCharacter(PC);
		}
		DCSaveWorld();
	}
	Super::Logout(Exiting);
}

void ADCSurvivalGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bWorldApplied)
	{
		DCSaveWorld();
		bWorldApplied = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ADCSurvivalGameMode::TickWorld()
{
	UpdateTimeScale();

	SecondsSinceAutosave += 1.f;
	if (SecondsSinceAutosave >= AutosaveIntervalSeconds)
	{
		DCSaveWorld();
	}
}

void ADCSurvivalGameMode::UpdateTimeScale()
{
	ADCSurvivalGameState* GS = GetGameState<ADCSurvivalGameState>();
	if (!GS)
	{
		return;
	}

	int32 Living = 0;
	int32 Sleeping = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ADCCharacter* Character = It->Get() ? It->Get()->GetPawn<ADCCharacter>() : nullptr;
		if (Character && !Character->IsDead())
		{
			++Living;
			Sleeping += Character->IsSleeping() ? 1 : 0;
		}
	}
	GS->SetTimeScale((Living > 0 && Sleeping == Living) ? SleepTimeScale : 1.f);
}

// ---------------------------------------------------------------------------
// Survivors
// ---------------------------------------------------------------------------

AActor* ADCSurvivalGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Survivors start somewhere random, not always at the same spot.
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(*It);
	}
	if (Starts.Num() > 0)
	{
		return Starts[FMath::RandRange(0, Starts.Num() - 1)];
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ADCSurvivalGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	if (ADCCharacter* Character = Cast<ADCCharacter>(PlayerPawn))
	{
		Character->GetSurvivalComponent()->HeatStressMultiplier = Region ? Region->HeatStressMultiplier : 1.f;
	}
}

void ADCSurvivalGameMode::SetupSurvivor(APlayerController* PC, bool bAllowRestore)
{
	ADCPlayerState* PS = PC ? PC->GetPlayerState<ADCPlayerState>() : nullptr;
	ADCCharacter* Character = PC ? PC->GetPawn<ADCCharacter>() : nullptr;
	if (!PS || !Character || !WorldSave)
	{
		return;
	}

	const FDCSavedCharacter* Saved = bAllowRestore ? WorldSave->Characters.Find(PS->GetSaveId()) : nullptr;
	if (Saved)
	{
		Character->ReadSaveData(*Saved);
		PC->SetControlRotation(Saved->ControlRotation);
		PS->SurvivorName = Saved->SurvivorName;
		PS->SurvivorStartGameHours = Saved->StartGameHours;
		PS->ZombieKills = Saved->ZombieKills;
		PS->bSurvivorDead = false;
		UE_LOG(LogDC, Log, TEXT("Welcome back, %s."), *PS->SurvivorName);
	}
	else
	{
		StartNewSurvivor(PC);
	}
}

void ADCSurvivalGameMode::StartNewSurvivor(APlayerController* PC)
{
	ADCPlayerState* PS = PC->GetPlayerState<ADCPlayerState>();
	const ADCSurvivalGameState* GS = GetGameState<ADCSurvivalGameState>();
	if (!PS || !GS)
	{
		return;
	}
	PS->SurvivorName = DCSurvivorNames::Make();
	PS->SurvivorStartGameHours = GS->GetTotalGameHours();
	PS->ZombieKills = 0;
	PS->bSurvivorDead = false;
	UE_LOG(LogDC, Log, TEXT("New survivor: %s (%s)."), *PS->SurvivorName, *GS->GetClockString());
}

void ADCSurvivalGameMode::HandleSurvivorDied(AController* Victim, AController* Killer)
{
	ADCPlayerState* PS = Victim ? Victim->GetPlayerState<ADCPlayerState>() : nullptr;
	const ADCSurvivalGameState* GS = GetGameState<ADCSurvivalGameState>();
	if (!PS || !GS)
	{
		return;
	}

	PS->bSurvivorDead = true;
	PS->LastDaysSurvived = PS->GetDaysSurvived(GS->GetTotalGameHours());
	if (WorldSave)
	{
		WorldSave->Characters.Remove(PS->GetSaveId());
	}
	UE_LOG(LogDC, Log, TEXT("%s died after %.1f days."), *PS->SurvivorName, PS->LastDaysSurvived);

	// Death is permanent - save straight away so it can't be undone by quitting.
	DCSaveWorld();
}

void ADCSurvivalGameMode::RespawnSurvivor(APlayerController* Player)
{
	ADCPlayerState* PS = Player ? Player->GetPlayerState<ADCPlayerState>() : nullptr;
	if (!PS || !PS->bSurvivorDead || Player->GetPawn())
	{
		return;
	}
	RestartPlayer(Player);
	StartNewSurvivor(Player);
}

void ADCSurvivalGameMode::HandleZombieKilled(AController* Killer)
{
	if (ADCPlayerState* PS = Killer ? Killer->GetPlayerState<ADCPlayerState>() : nullptr)
	{
		++PS->ZombieKills;
	}
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

void ADCSurvivalGameMode::DCSaveWorld()
{
	if (!WorldSave || !bWorldApplied)
	{
		return;
	}
	CaptureWorldState();
	WriteWorldToDisk();
	SecondsSinceAutosave = 0.f;
}

void ADCSurvivalGameMode::LoadWorldFromDisk()
{
	const FString Slot = GetSlotName();
	UDCWorldSave* Loaded = nullptr;

	for (const FString& Candidate : { Slot, Slot + TEXT("_Backup") })
	{
		if (UGameplayStatics::DoesSaveGameExist(Candidate, 0))
		{
			Loaded = Cast<UDCWorldSave>(UGameplayStatics::LoadGameFromSlot(Candidate, 0));
			if (Loaded)
			{
				if (Candidate != Slot)
				{
					UE_LOG(LogDC, Warning, TEXT("Main save unreadable; loaded backup %s."), *Candidate);
				}
				break;
			}
		}
	}

	if (Loaded && Loaded->SaveVersion > DC_WORLD_SAVE_VERSION)
	{
		// Never overwrite a save made by a newer build: play this session in a separate world.
		UE_LOG(LogDC, Error, TEXT("World '%s' was saved by a newer version (%d > %d). Starting '%s_Unsupported' instead."),
			*WorldName, Loaded->SaveVersion, DC_WORLD_SAVE_VERSION, *WorldName);
		WorldName += TEXT("_Unsupported");
		Loaded = nullptr;
	}

	if (Loaded)
	{
		WorldSave = Loaded;
		UE_LOG(LogDC, Log, TEXT("Loaded world '%s' (saved %s, %d survivors, %d containers, %d zombies)."),
			*WorldName, *Loaded->SavedAt.ToString(), Loaded->Characters.Num(), Loaded->Containers.Num(), Loaded->Zombies.Num());
	}
	else
	{
		WorldSave = NewObject<UDCWorldSave>(this);
		WorldSave->WorldName = WorldName;
		WorldSave->GameHours = -1.0; // marks a brand-new world
		UE_LOG(LogDC, Log, TEXT("Starting new world '%s'."), *WorldName);
	}
}

void ADCSurvivalGameMode::ApplyWorldState()
{
	ADCSurvivalGameState* GS = GetGameState<ADCSurvivalGameState>();
	if (!GS || !WorldSave)
	{
		return;
	}

	if (WorldSave->GameHours < 0.0)
	{
		// New world: keep the level as authored; start on Day 1 at the region's start hour.
		GS->SetTotalGameHours(Region ? Region->StartTimeOfDayHours : 7.0);
		return;
	}

	GS->SetTotalGameHours(WorldSave->GameHours);
	UWorld* World = GetWorld();

	// Containers: placed ones keep their identity, spawned ones (bodies) are recreated.
	TMap<FName, ADCLootContainer*> Placed;
	for (TActorIterator<ADCLootContainer> It(World); It; ++It)
	{
		if (It->IsNetStartupActor())
		{
			Placed.Add(It->GetFName(), *It);
		}
	}
	for (const FDCSavedContainer& Saved : WorldSave->Containers)
	{
		ADCLootContainer* Container = nullptr;
		if (!Saved.bSpawned)
		{
			Container = Placed.FindRef(Saved.ActorName);
		}
		else
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Container = World->SpawnActor<ADCLootContainer>(ADCLootContainer::StaticClass(), Saved.Transform, Params);
			if (Container)
			{
				Container->ContainerName = Saved.DisplayName;
				Container->bDestroyWhenEmpty = Saved.bDestroyWhenEmpty;
				Container->GetInventoryComponent()->GridWidth = Saved.GridWidth;
				Container->GetInventoryComponent()->GridHeight = Saved.GridHeight;
			}
		}
		if (Container)
		{
			Container->GetInventoryComponent()->ImportSave(Saved.Entries);
		}
	}

	// Loose items: replace whatever the level placed with exactly what was saved.
	for (TActorIterator<ADCWorldItem> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (const FDCSavedWorldItem& Saved : WorldSave->WorldItems)
	{
		if (UDCItemDefinition* Item = Cast<UDCItemDefinition>(Saved.Item.TryLoad()))
		{
			ADCWorldItem::SpawnDropped(World, Item, Saved.Count, Saved.Transform.GetLocation(), Saved.Transform.Rotator());
		}
	}

	// Zombies: the dead stay dead, the living are where you left them.
	for (TActorIterator<ADCZombie> It(World); It; ++It)
	{
		if (AController* AI = It->GetController())
		{
			AI->Destroy();
		}
		It->Destroy();
	}
	for (const FDCSavedZombie& Saved : WorldSave->Zombies)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (ADCZombie* Zombie = World->SpawnActor<ADCZombie>(ADCZombie::StaticClass(), Saved.Transform, Params))
		{
			Zombie->SetHealth(Saved.Health);
		}
	}

	UE_LOG(LogDC, Log, TEXT("World state applied: %s."), *GS->GetClockString());
}

void ADCSurvivalGameMode::CaptureCharacter(APlayerController* PC)
{
	const ADCPlayerState* PS = PC ? PC->GetPlayerState<ADCPlayerState>() : nullptr;
	const ADCCharacter* Character = PC ? PC->GetPawn<ADCCharacter>() : nullptr;
	if (!PS || !Character || Character->IsDead() || PS->bSurvivorDead || !WorldSave)
	{
		return;
	}

	FDCSavedCharacter Saved;
	Character->WriteSaveData(Saved);
	Saved.SurvivorName = PS->SurvivorName;
	Saved.StartGameHours = PS->SurvivorStartGameHours;
	Saved.ZombieKills = PS->ZombieKills;
	Saved.ControlRotation = PC->GetControlRotation();
	WorldSave->Characters.Add(PS->GetSaveId(), Saved);
}

void ADCSurvivalGameMode::CaptureWorldState()
{
	UWorld* World = GetWorld();
	if (const ADCSurvivalGameState* GS = GetGameState<ADCSurvivalGameState>())
	{
		WorldSave->GameHours = GS->GetTotalGameHours();
	}

	WorldSave->Containers.Reset();
	for (TActorIterator<ADCLootContainer> It(World); It; ++It)
	{
		ADCLootContainer* Container = *It;
		if (!IsValid(Container) || Container->IsActorBeingDestroyed())
		{
			continue;
		}
		FDCSavedContainer& Saved = WorldSave->Containers.AddDefaulted_GetRef();
		Saved.ActorName = Container->GetFName();
		Saved.bSpawned = !Container->IsNetStartupActor();
		Saved.Transform = Container->GetActorTransform();
		Saved.DisplayName = Container->ContainerName;
		Saved.bDestroyWhenEmpty = Container->bDestroyWhenEmpty;
		Saved.GridWidth = Container->GetInventoryComponent()->GridWidth;
		Saved.GridHeight = Container->GetInventoryComponent()->GridHeight;
		Container->GetInventoryComponent()->ExportSave(Saved.Entries);
	}

	WorldSave->WorldItems.Reset();
	for (TActorIterator<ADCWorldItem> It(World); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->GetItem())
		{
			FDCSavedWorldItem& Saved = WorldSave->WorldItems.AddDefaulted_GetRef();
			Saved.Item = FSoftObjectPath(It->GetItem());
			Saved.Count = It->GetCount();
			Saved.Transform = It->GetActorTransform();
		}
	}

	WorldSave->Zombies.Reset();
	for (TActorIterator<ADCZombie> It(World); It; ++It)
	{
		if (IsValid(*It) && !It->IsDead() && !It->IsActorBeingDestroyed())
		{
			FDCSavedZombie& Saved = WorldSave->Zombies.AddDefaulted_GetRef();
			Saved.Transform = It->GetActorTransform();
			Saved.Health = It->GetHealth();
		}
	}

	// Connected survivors; disconnected players' survivors stay in the map untouched.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		CaptureCharacter(It->Get());
	}
}

void ADCSurvivalGameMode::WriteWorldToDisk()
{
	WorldSave->SaveVersion = DC_WORLD_SAVE_VERSION;
	WorldSave->WorldName = WorldName;
	WorldSave->SavedAt = FDateTime::UtcNow();

	const FString Slot = GetSlotName();
	const bool bOk = UGameplayStatics::SaveGameToSlot(WorldSave, Slot, 0);

	// Every third save also refreshes the backup, so a bad save never takes the only copy.
	if (bOk && (++SaveCount % 3) == 1)
	{
		UGameplayStatics::SaveGameToSlot(WorldSave, Slot + TEXT("_Backup"), 0);
	}

	UE_LOG(LogDC, Log, TEXT("World '%s' %s (%d survivors, %d containers, %d items, %d zombies)."),
		*WorldName, bOk ? TEXT("saved") : TEXT("FAILED TO SAVE"),
		WorldSave->Characters.Num(), WorldSave->Containers.Num(), WorldSave->WorldItems.Num(), WorldSave->Zombies.Num());
}
