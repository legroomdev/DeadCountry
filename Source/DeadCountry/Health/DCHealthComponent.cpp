#include "Health/DCHealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

UDCHealthComponent::UDCHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(true);
}

void UDCHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDCHealthComponent, BodyParts);
	DOREPLIFETIME(UDCHealthComponent, Infection);
	DOREPLIFETIME(UDCHealthComponent, bInfected);
	DOREPLIFETIME(UDCHealthComponent, bDead);
}

void UDCHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		InitBodyParts();
	}
}

bool UDCHealthComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

void UDCHealthComponent::InitBodyParts()
{
	auto Make = [](EDCBodyPart Part, float Max)
	{
		FDCBodyPartState State;
		State.Part = Part;
		State.Health = Max;
		State.MaxHealth = Max;
		return State;
	};

	BodyParts.Reset();
	BodyParts.Add(Make(EDCBodyPart::Head, HeadMax));
	BodyParts.Add(Make(EDCBodyPart::Thorax, ThoraxMax));
	BodyParts.Add(Make(EDCBodyPart::Stomach, StomachMax));
	BodyParts.Add(Make(EDCBodyPart::LeftArm, ArmMax));
	BodyParts.Add(Make(EDCBodyPart::RightArm, ArmMax));
	BodyParts.Add(Make(EDCBodyPart::LeftLeg, LegMax));
	BodyParts.Add(Make(EDCBodyPart::RightLeg, LegMax));
	OnHealthChanged.Broadcast();
}

FDCBodyPartState* UDCHealthComponent::FindPart(EDCBodyPart Part)
{
	return BodyParts.FindByPredicate([Part](const FDCBodyPartState& S) { return S.Part == Part; });
}

FDCBodyPartState UDCHealthComponent::GetPart(EDCBodyPart Part) const
{
	const FDCBodyPartState* Found = BodyParts.FindByPredicate([Part](const FDCBodyPartState& S) { return S.Part == Part; });
	return Found ? *Found : FDCBodyPartState();
}

void UDCHealthComponent::RestoreState(const TArray<FDCBodyPartState>& Parts, float InInfection, bool bInInfected)
{
	if (!HasAuthority())
	{
		return;
	}
	// Only take parts we recognise, so a save from an older layout can't break the body.
	for (const FDCBodyPartState& Saved : Parts)
	{
		if (FDCBodyPartState* State = FindPart(Saved.Part))
		{
			State->Health = FMath::Clamp(Saved.Health, 0.f, State->MaxHealth);
			State->bBleeding = Saved.bBleeding;
			State->bFractured = Saved.bFractured;
		}
	}
	Infection = FMath::Clamp(InInfection, 0.f, 1.f);
	bInfected = bInInfected;
	OnHealthChanged.Broadcast();
}

void UDCHealthComponent::ApplyDamageToPart(EDCBodyPart Part, float Amount, AController* DamageInstigator, float BleedChance, float FractureChance, bool bInfect)
{
	if (!HasAuthority() || bDead || Amount <= 0.f)
	{
		return;
	}

	FDCBodyPartState* State = FindPart(Part);
	if (!State)
	{
		return;
	}

	if (DamageInstigator)
	{
		LastAttackedTime = GetWorld()->GetTimeSeconds();
	}

	if (State->IsDestroyed())
	{
		// Blacked-out limb: spread damage across everything still alive.
		TArray<FDCBodyPartState*> Alive;
		for (FDCBodyPartState& Other : BodyParts)
		{
			if (!Other.IsDestroyed())
			{
				Alive.Add(&Other);
			}
		}
		const float Share = Alive.Num() > 0 ? (Amount * DestroyedLimbSpreadMultiplier) / Alive.Num() : 0.f;
		for (FDCBodyPartState* Other : Alive)
		{
			Other->Health = FMath::Max(0.f, Other->Health - Share);
		}
	}
	else
	{
		State->Health = FMath::Max(0.f, State->Health - Amount);
	}

	if (FMath::FRand() < BleedChance)
	{
		State->bBleeding = true;
	}
	const bool bIsLimb = Part != EDCBodyPart::Head && Part != EDCBodyPart::Thorax && Part != EDCBodyPart::Stomach;
	if (bIsLimb && FMath::FRand() < FractureChance)
	{
		State->bFractured = true;
	}
	if (bInfect && !bInfected)
	{
		bInfected = true;
	}

	OnHealthChanged.Broadcast();
	CheckDeath(DamageInstigator);
}

void UDCHealthComponent::Heal(float Amount)
{
	if (!HasAuthority() || bDead || Amount <= 0.f)
	{
		return;
	}

	while (Amount > KINDA_SMALL_NUMBER)
	{
		FDCBodyPartState* Worst = nullptr;
		float WorstMissing = 0.f;
		for (FDCBodyPartState& State : BodyParts)
		{
			const float Missing = State.MaxHealth - State.Health;
			if (!State.IsDestroyed() && Missing > WorstMissing)
			{
				Worst = &State;
				WorstMissing = Missing;
			}
		}
		if (!Worst)
		{
			break;
		}
		const float Applied = FMath::Min(Amount, WorstMissing);
		Worst->Health += Applied;
		Amount -= Applied;
	}
	OnHealthChanged.Broadcast();
}

bool UDCHealthComponent::StopOneBleed()
{
	if (!HasAuthority())
	{
		return false;
	}
	for (FDCBodyPartState& State : BodyParts)
	{
		if (State.bBleeding)
		{
			State.bBleeding = false;
			OnHealthChanged.Broadcast();
			return true;
		}
	}
	return false;
}

bool UDCHealthComponent::SplintOneFracture()
{
	if (!HasAuthority())
	{
		return false;
	}
	for (FDCBodyPartState& State : BodyParts)
	{
		if (State.bFractured)
		{
			State.bFractured = false;
			OnHealthChanged.Broadcast();
			return true;
		}
	}
	return false;
}

void UDCHealthComponent::Kill(AController* Killer)
{
	if (!HasAuthority() || bDead)
	{
		return;
	}
	bDead = true;
	OnHealthChanged.Broadcast();
	OnDeath.Broadcast(Killer);
}

void UDCHealthComponent::CheckDeath(AController* DamageInstigator)
{
	const FDCBodyPartState* Head = FindPart(EDCBodyPart::Head);
	const FDCBodyPartState* Thorax = FindPart(EDCBodyPart::Thorax);
	if ((Head && Head->IsDestroyed()) || (Thorax && Thorax->IsDestroyed()))
	{
		Kill(DamageInstigator);
	}
}

void UDCHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HasAuthority() || bDead)
	{
		return;
	}

	bool bChanged = false;

	// Bleeds drain the bleeding limb; once destroyed, the blood loss hits the thorax.
	for (FDCBodyPartState& State : BodyParts)
	{
		if (State.bBleeding)
		{
			FDCBodyPartState* Target = State.IsDestroyed() ? FindPart(EDCBodyPart::Thorax) : &State;
			if (Target)
			{
				Target->Health = FMath::Max(0.f, Target->Health - BleedDamagePerSecond * DeltaTime);
				bChanged = true;
			}
		}
	}

	if (bInfected)
	{
		Infection = FMath::Min(1.f, Infection + DeltaTime / FMath::Max(1.f, InfectionSecondsToTurn));
		bChanged = true;
		if (Infection >= 1.f)
		{
			Kill(nullptr);
			return;
		}
	}

	if (bChanged)
	{
		OnHealthChanged.Broadcast();
		CheckDeath(nullptr);
	}
}

float UDCHealthComponent::GetTotalHealth() const
{
	float Total = 0.f;
	for (const FDCBodyPartState& State : BodyParts)
	{
		Total += State.Health;
	}
	return Total;
}

float UDCHealthComponent::GetTotalMaxHealth() const
{
	float Total = 0.f;
	for (const FDCBodyPartState& State : BodyParts)
	{
		Total += State.MaxHealth;
	}
	return Total;
}

bool UDCHealthComponent::IsBleeding() const
{
	return BodyParts.ContainsByPredicate([](const FDCBodyPartState& S) { return S.bBleeding; });
}

bool UDCHealthComponent::HasLegInjury() const
{
	return BodyParts.ContainsByPredicate([](const FDCBodyPartState& S)
	{
		const bool bLeg = S.Part == EDCBodyPart::LeftLeg || S.Part == EDCBodyPart::RightLeg;
		return bLeg && (S.bFractured || S.IsDestroyed());
	});
}

void UDCHealthComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast();
}

EDCBodyPart UDCHealthComponent::BodyPartFromCapsuleHit(const AActor* Victim, const FVector& WorldHitLocation)
{
	const ACharacter* Character = Cast<ACharacter>(Victim);
	if (!Character || !Character->GetCapsuleComponent())
	{
		return EDCBodyPart::Thorax;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FVector Local = Capsule->GetComponentTransform().InverseTransformPosition(WorldHitLocation);
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Height01 = FMath::Clamp((Local.Z + HalfHeight) / (2.f * HalfHeight), 0.f, 1.f);
	const bool bLeftSide = Local.Y < 0.f;
	const bool bOuter = FMath::Abs(Local.Y) > Capsule->GetScaledCapsuleRadius() * 0.6f;

	if (Height01 > 0.86f) return EDCBodyPart::Head;
	if (Height01 > 0.62f) return bOuter ? (bLeftSide ? EDCBodyPart::LeftArm : EDCBodyPart::RightArm) : EDCBodyPart::Thorax;
	if (Height01 > 0.46f) return bOuter ? (bLeftSide ? EDCBodyPart::LeftArm : EDCBodyPart::RightArm) : EDCBodyPart::Stomach;
	return bLeftSide ? EDCBodyPart::LeftLeg : EDCBodyPart::RightLeg;
}

FText UDCHealthComponent::GetBodyPartName(EDCBodyPart Part)
{
	switch (Part)
	{
	case EDCBodyPart::Head:     return NSLOCTEXT("DC", "Head", "Head");
	case EDCBodyPart::Thorax:   return NSLOCTEXT("DC", "Thorax", "Thorax");
	case EDCBodyPart::Stomach:  return NSLOCTEXT("DC", "Stomach", "Stomach");
	case EDCBodyPart::LeftArm:  return NSLOCTEXT("DC", "LArm", "L Arm");
	case EDCBodyPart::RightArm: return NSLOCTEXT("DC", "RArm", "R Arm");
	case EDCBodyPart::LeftLeg:  return NSLOCTEXT("DC", "LLeg", "L Leg");
	case EDCBodyPart::RightLeg: return NSLOCTEXT("DC", "RLeg", "R Leg");
	default:                     return FText::GetEmpty();
	}
}
