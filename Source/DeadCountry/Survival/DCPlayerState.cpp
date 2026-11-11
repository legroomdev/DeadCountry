#include "Survival/DCPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

void ADCPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCPlayerState, SurvivorName);
	DOREPLIFETIME(ADCPlayerState, SurvivorStartGameHours);
	DOREPLIFETIME(ADCPlayerState, ZombieKills);
	DOREPLIFETIME(ADCPlayerState, bSurvivorDead);
	DOREPLIFETIME(ADCPlayerState, LastDaysSurvived);
}

FString ADCPlayerState::GetSaveId() const
{
	// The host / single player always maps to the same slot, whatever the online subsystem says.
	const APlayerController* PC = GetPlayerController();
	if (PC && PC->IsLocalController())
	{
		return TEXT("LocalHost");
	}
	// Remote players: their platform id (Steam/EOS later). The NULL subsystem may give a new id
	// each session, which simply means a new survivor - acceptable until online services are in.
	const FUniqueNetIdRepl& Id = GetUniqueId();
	return Id.IsValid() ? Id.ToString() : GetPlayerName();
}
