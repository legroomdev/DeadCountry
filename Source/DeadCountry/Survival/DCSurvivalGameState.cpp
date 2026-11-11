#include "Survival/DCSurvivalGameState.h"
#include "World/DCRegionDefinition.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ADCSurvivalGameState::ADCSurvivalGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	SetNetUpdateFrequency(2.f);
}

void ADCSurvivalGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCSurvivalGameState, ReplicatedGameHours);
	DOREPLIFETIME(ADCSurvivalGameState, TimeScale);
	DOREPLIFETIME(ADCSurvivalGameState, Region);
}

void ADCSurvivalGameState::BeginPlay()
{
	Super::BeginPlay();
	FindSkyLights();
	UpdateSky();
}

float ADCSurvivalGameState::GetGameHoursPerSecond() const
{
	return 24.f / (FMath::Max(1.f, DayLengthMinutes) * 60.f) * TimeScale;
}

void ADCSurvivalGameState::SetTotalGameHours(double Hours)
{
	TotalGameHours = Hours;
	ReplicatedGameHours = Hours;
	UpdateSky();
}

void ADCSurvivalGameState::SetTimeScale(float NewScale)
{
	TimeScale = FMath::Max(0.f, NewScale);
}

void ADCSurvivalGameState::OnRep_Clock()
{
	// Snap only if we've drifted noticeably (> ~2 in-game minutes); otherwise keep the smooth local clock.
	if (FMath::Abs(TotalGameHours - ReplicatedGameHours) > 0.035)
	{
		TotalGameHours = ReplicatedGameHours;
	}
}

void ADCSurvivalGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TotalGameHours += DeltaSeconds * GetGameHoursPerSecond();

	if (HasAuthority())
	{
		// Push the authoritative clock a couple of times a second, not every frame.
		ReplicateAccumulator += DeltaSeconds;
		if (ReplicateAccumulator >= 0.5f)
		{
			ReplicateAccumulator = 0.f;
			ReplicatedGameHours = TotalGameHours;
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateSky();
	}
}

FString ADCSurvivalGameState::GetClockString() const
{
	const float Hour = GetHourOfDay();
	const int32 H = FMath::FloorToInt(Hour);
	const int32 M = FMath::FloorToInt((Hour - H) * 60.f);
	return FString::Printf(TEXT("Day %d  %02d:%02d"), GetDay(), H, M);
}

void ADCSurvivalGameState::FindSkyLights()
{
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("Moon")))
		{
			Moon = *It;
		}
		else if (!Sun.IsValid())
		{
			Sun = *It;
		}
	}
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		Fog = *It;
		FogDayColor = It->GetComponent()->FogInscatteringLuminance;
		break;
	}
}

void ADCSurvivalGameState::UpdateSky()
{
	// The sun turns a full circle per day: horizon at 06:00, overhead at 12:00, horizon at 18:00.
	const float Hour = GetHourOfDay();
	const float SunPitch = -(Hour - 6.f) / 24.f * 360.f;
	const float Yaw = 35.f;

	if (ADirectionalLight* SunLight = Sun.Get())
	{
		SunLight->SetActorRotation(FRotator(SunPitch, Yaw, 0.f));
	}
	if (ADirectionalLight* MoonLight = Moon.Get())
	{
		MoonLight->SetActorRotation(FRotator(SunPitch + 180.f, Yaw, 0.f));
		// Only let the moon light the world while the sun is down.
		const bool bSunDown = Hour < 6.f || Hour > 18.f;
		MoonLight->GetLightComponent()->SetVisibility(bSunDown);
	}
	if (AExponentialHeightFog* FogActor = Fog.Get())
	{
		// The fog's glow is a constant emissive colour, so fade it with the sun or nights glow orange.
		const float SunElevation = FMath::Sin((Hour - 6.f) / 24.f * 2.f * PI); // 1 at noon, -1 at midnight
		const float Daylight = FMath::Clamp(SunElevation * 4.f + 0.25f, 0.02f, 1.f);
		FogActor->GetComponent()->SetFogInscatteringColor(FogDayColor * Daylight);
	}
}
