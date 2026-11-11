#include "Weapons/DCWeapon.h"
#include "DeadCountry.h"
#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "Player/DCCharacter.h"
#include "Core/DCAudio.h"
#include "Weapons/DCShellCasing.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace DCWeaponP9
{
	// Pivots measured on the P9 model (cm, weapon space).
	const FVector TriggerPivot(10.2f, 0.f, -1.2f);
	const FVector BarrelLugPivot(10.0f, 0.f, 0.4f);
	const float RecoilSpringRearX = 10.35f;
	const float RecoilSpringLength = 7.55f;
	const float MagDropCm = 16.f;
	const FName MuzzleSocket(TEXT("Muzzle"));
	const FName EjectSocket(TEXT("Eject"));
	const float FlashSeconds = 0.045f;

	enum ESurface : uint8 { SurfaceNone = 0, SurfaceWorld = 1, SurfaceFlesh = 2 };

	FTransform RotateAbout(const FVector& Pivot, const FRotator& Rotation, const FVector& Offset = FVector::ZeroVector)
	{
		const FQuat Q = Rotation.Quaternion();
		return FTransform(Q, Pivot - Q.RotateVector(Pivot) + Offset);
	}
}

static UStaticMeshComponent* MakePart(AActor* Owner, const TCHAR* Name, const TCHAR* MeshPath, USceneComponent* Parent)
{
	UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->CastShadow = false;
	if (Parent)
	{
		Comp->SetupAttachment(Parent);
	}
	ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(MeshPath);
	if (Finder.Succeeded())
	{
		Comp->SetStaticMesh(Finder.Object);
	}
	return Comp;
}

ADCWeapon::ADCWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	FrameMesh = MakePart(this, TEXT("Frame"), TEXT("/Game/Weapons/P9/SM_P9_Frame.SM_P9_Frame"), nullptr);
	RootComponent = FrameMesh;

	// Slide group: everything that rides with the slide.
	SlideMesh = MakePart(this, TEXT("Slide"), TEXT("/Game/Weapons/P9/SM_P9_Slide.SM_P9_Slide"), FrameMesh);
	MakePart(this, TEXT("Extractor"), TEXT("/Game/Weapons/P9/SM_P9_Extractor.SM_P9_Extractor"), SlideMesh);
	MakePart(this, TEXT("Striker"), TEXT("/Game/Weapons/P9/SM_P9_Striker.SM_P9_Striker"), SlideMesh);
	MakePart(this, TEXT("StrikerSpring"), TEXT("/Game/Weapons/P9/SM_P9_StrikerSpring.SM_P9_StrikerSpring"), SlideMesh);

	BarrelMesh = MakePart(this, TEXT("Barrel"), TEXT("/Game/Weapons/P9/SM_P9_Barrel.SM_P9_Barrel"), FrameMesh);
	MakePart(this, TEXT("GuideRod"), TEXT("/Game/Weapons/P9/SM_P9_GuideRod.SM_P9_GuideRod"), FrameMesh);
	RecoilSpringMesh = MakePart(this, TEXT("RecoilSpring"), TEXT("/Game/Weapons/P9/SM_P9_RecoilSpring.SM_P9_RecoilSpring"), FrameMesh);

	TriggerMesh = MakePart(this, TEXT("Trigger"), TEXT("/Game/Weapons/P9/SM_P9_Trigger.SM_P9_Trigger"), FrameMesh);
	MakePart(this, TEXT("TriggerSafety"), TEXT("/Game/Weapons/P9/SM_P9_TriggerSafety.SM_P9_TriggerSafety"), TriggerMesh);

	// Fixed frame parts.
	MakePart(this, TEXT("TriggerBar"), TEXT("/Game/Weapons/P9/SM_P9_TriggerBar.SM_P9_TriggerBar"), FrameMesh);
	MakePart(this, TEXT("SlideStop"), TEXT("/Game/Weapons/P9/SM_P9_SlideStop.SM_P9_SlideStop"), FrameMesh);
	MakePart(this, TEXT("TakedownLever"), TEXT("/Game/Weapons/P9/SM_P9_TakedownLever.SM_P9_TakedownLever"), FrameMesh);
	MakePart(this, TEXT("MagCatch"), TEXT("/Game/Weapons/P9/SM_P9_MagCatch.SM_P9_MagCatch"), FrameMesh);
	MakePart(this, TEXT("Pins"), TEXT("/Game/Weapons/P9/SM_P9_Pins.SM_P9_Pins"), FrameMesh);
	MakePart(this, TEXT("LockingBlock"), TEXT("/Game/Weapons/P9/SM_P9_LockingBlock.SM_P9_LockingBlock"), FrameMesh);

	MagazineMesh = MakePart(this, TEXT("Magazine"), TEXT("/Game/Weapons/P9/SM_P9_Magazine.SM_P9_Magazine"), FrameMesh);
	RoundMeshes.Add(MakePart(this, TEXT("Round0"), TEXT("/Game/Weapons/P9/SM_P9_Round0.SM_P9_Round0"), MagazineMesh));
	RoundMeshes.Add(MakePart(this, TEXT("Round1"), TEXT("/Game/Weapons/P9/SM_P9_Round1.SM_P9_Round1"), MagazineMesh));

	// Muzzle flash: a quick orange light plus two crossed emissive cards.
	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(FrameMesh, DCWeaponP9::MuzzleSocket);
	MuzzleLight->SetRelativeLocation(FVector(4.f, 0.f, 0.f));
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.62f, 0.3f));
	MuzzleLight->SetAttenuationRadius(700.f);
	MuzzleLight->SetCastShadows(false);
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMat(TEXT("/Game/VFX/M_MuzzleFlash.M_MuzzleFlash"));
	auto MakeFlash = [&](const TCHAR* Name, const FRotator& Rot, const FVector& Scale, const FVector& Offset)
	{
		UStaticMeshComponent* Card = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Card->SetupAttachment(FrameMesh, DCWeaponP9::MuzzleSocket);
		Card->SetStaticMesh(Plane.Object);
		if (FlashMat.Succeeded())
		{
			Card->SetMaterial(0, FlashMat.Object);
		}
		Card->SetRelativeRotation(Rot);
		Card->SetRelativeScale3D(Scale);
		Card->SetRelativeLocation(Offset);
		Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Card->CastShadow = false;
		Card->SetVisibility(false);
		return Card;
	};
	FlashFront = MakeFlash(TEXT("FlashFront"), FRotator(90.f, 0.f, 0.f), FVector(0.07f), FVector(1.f, 0.f, 0.f));
	FlashSide = MakeFlash(TEXT("FlashSide"), FRotator(0.f, 0.f, 90.f), FVector(0.13f, 0.06f, 1.f), FVector(6.f, 0.f, 0.f));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HoleMat(TEXT("/Game/VFX/M_Decal_BulletHole.M_Decal_BulletHole"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BloodMat(TEXT("/Game/VFX/M_Decal_Blood.M_Decal_Blood"));
	BulletHoleDecal = HoleMat.Object;
	BloodDecal = BloodMat.Object;

	WeaponName = NSLOCTEXT("DC", "P9Name", "P9 Service Pistol");
	AmmoItem = TSoftObjectPtr<UDCItemDefinition>(FSoftObjectPath(TEXT("/Game/Items/DA_Ammo9mm.DA_Ammo9mm")));
}

void ADCWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCWeapon, AmmoInMagazine);
	DOREPLIFETIME(ADCWeapon, bReloading);
}

void ADCWeapon::BeginPlay()
{
	Super::BeginPlay();
	if (!AmmoItem.IsNull())
	{
		ResolvedAmmoItem = AmmoItem.LoadSynchronous();
	}
	if (HasAuthority())
	{
		AmmoInMagazine = MagazineSize;
	}
}

int32 ADCWeapon::GetReserveAmmo() const
{
	if (!ResolvedAmmoItem)
	{
		return 999;
	}
	const ADCCharacter* Character = Cast<ADCCharacter>(GetOwner());
	return Character ? Character->GetInventoryComponent()->CountItem(ResolvedAmmoItem) : 0;
}

// ---------------------------------------------------------------------------
// Firing
// ---------------------------------------------------------------------------

void ADCWeapon::StartFire()
{
	bTriggerHeld = true;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Wait = FMath::Max(0.f, LastLocalFireTime + GetFireInterval() - Now);

	if (bAutomatic)
	{
		GetWorldTimerManager().SetTimer(AutoFireTimer, this, &ADCWeapon::LocalFireShot, GetFireInterval(), true, Wait);
	}
	else if (Wait <= 0.f)
	{
		LocalFireShot();
	}
}

void ADCWeapon::StopFire()
{
	bTriggerHeld = false;
	GetWorldTimerManager().ClearTimer(AutoFireTimer);
}

void ADCWeapon::LocalFireShot()
{
	ADCCharacter* Character = Cast<ADCCharacter>(GetOwner());
	if (!Character || Character->IsDead() || bReloading)
	{
		StopFire();
		return;
	}
	if (AmmoInMagazine <= 0)
	{
		StopFire();
		DCAudio::PlayAt(this, EDCSound::DryFire, GetActorLocation());
		RequestReload();
		return;
	}

	LastLocalFireTime = GetWorld()->GetTimeSeconds();
	PlayShotEffects(); // instant feedback for the shooter; others get it from the multicast

	FVector ViewLocation;
	FRotator ViewRotation;
	Character->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	Server_Fire(ViewLocation, ViewRotation.Vector());

	// View recoil is applied locally, after the shot direction was taken, so it affects the next shot.
	const float Scale = Character->IsAiming() ? AimRecoilMultiplier : 1.f;
	const float Yaw = FMath::FRandRange(-RecoilYaw, RecoilYaw * 1.4f) * Scale;
	Character->ApplyRecoil(RecoilPitch * Scale * FMath::FRandRange(0.85f, 1.15f), Yaw, RecoilRecoverFraction);
}

void ADCWeapon::GetVisualRecoil(float AimAlpha, FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	const float T = GetWorld()->GetTimeSeconds() - GetLastShotTime();
	if (T < 0.f || T > 0.4f)
	{
		return;
	}
	// Sharp 25ms rise, then a damped settle.
	const float Kick = T < 0.025f ? T / 0.025f : FMath::Exp(-(T - 0.025f) * 16.f);
	const float FlipScale = FMath::Lerp(1.f, AimKickMultiplier, AimAlpha);
	OutLocation = FVector(-KickBackCm * Kick, 0.f, 0.25f * Kick);
	OutRotation = FRotator(MuzzleFlipDegrees * FlipScale * Kick, 0.f, 0.f);
}

void ADCWeapon::Server_Fire_Implementation(FVector_NetQuantize ClientViewLocation, FVector_NetQuantizeNormal ClientViewDirection)
{
	ADCCharacter* Character = Cast<ADCCharacter>(GetOwner());
	if (!Character || Character->IsDead() || bReloading || AmmoInMagazine <= 0)
	{
		return;
	}

	// Rate limit with a little tolerance for network jitter.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastServerFireTime < GetFireInterval() * 0.75f)
	{
		return;
	}
	LastServerFireTime = Now;

	// Don't trust a view origin far from where the server thinks the pawn is.
	FVector ServerEyes;
	FRotator ServerRot;
	Character->GetActorEyesViewPoint(ServerEyes, ServerRot);
	const FVector Start = FVector::DistSquared(ServerEyes, ClientViewLocation) < FMath::Square(150.f) ? FVector(ClientViewLocation) : ServerEyes;

	--AmmoInMagazine;

	const float Spread = SpreadDegrees * (Character->IsAiming() ? AimSpreadMultiplier : 1.f);
	const FVector Direction = FMath::VRandCone(ClientViewDirection, FMath::DegreesToRadians(Spread));
	const FVector End = Start + Direction * RangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCWeaponTrace), true);
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Character);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, DC_TRACE_WEAPON, Params);
	if (bHit && Hit.GetActor())
	{
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Direction, Hit, Character->GetController(), this, nullptr);
	}

	// Zombies hear this. Loudness 1 at the muzzle.
	MakeNoise(1.f, Character, Start, NoiseRangeCm);

	uint8 Surface = DCWeaponP9::SurfaceNone;
	if (bHit)
	{
		Surface = Cast<APawn>(Hit.GetActor()) ? DCWeaponP9::SurfaceFlesh : DCWeaponP9::SurfaceWorld;
	}
	Multicast_FireEffects(bHit ? FVector(Hit.ImpactPoint) : End, bHit ? FVector(Hit.ImpactNormal) : -Direction, Surface);
}

bool ADCWeapon::IsOwnerLocallyControlled() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

void ADCWeapon::Multicast_FireEffects_Implementation(FVector_NetQuantize End, FVector_NetQuantizeNormal Normal, uint8 Surface)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// The shooter already played the report/casing the instant they pulled the trigger.
	if (!IsOwnerLocallyControlled())
	{
		LastFireVisualTime = GetWorld()->GetTimeSeconds();
		PlayShotEffects();
	}
	PlayImpactEffects(End, Normal, Surface);
}

void ADCWeapon::PlayShotEffects()
{
	const FVector Muzzle = FrameMesh->DoesSocketExist(DCWeaponP9::MuzzleSocket)
		? FrameMesh->GetSocketLocation(DCWeaponP9::MuzzleSocket)
		: GetActorLocation();
	DCAudio::PlayAt(this, EDCSound::Gunshot, Muzzle);

	if (FrameMesh->DoesSocketExist(DCWeaponP9::EjectSocket))
	{
		const AActor* Shooter = GetOwner();
		ADCShellCasing::Eject(GetWorld(), FrameMesh->GetSocketTransform(DCWeaponP9::EjectSocket),
			Shooter ? Shooter->GetVelocity() : FVector::ZeroVector);
	}
}

void ADCWeapon::PlayImpactEffects(const FVector& End, const FVector& Normal, uint8 Surface)
{
	if (Surface == DCWeaponP9::SurfaceNone)
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Muzzle = FrameMesh->DoesSocketExist(DCWeaponP9::MuzzleSocket)
		? FrameMesh->GetSocketLocation(DCWeaponP9::MuzzleSocket) : GetActorLocation();
	const FVector ShotDir = (End - Muzzle).GetSafeNormal();

	auto SpawnDecal = [World](UMaterialInterface* Mat, const FVector& Where, const FVector& SurfaceNormal, float Size, float Life)
	{
		if (!Mat)
		{
			return;
		}
		FRotator Rot = (-SurfaceNormal).Rotation();
		Rot.Roll = FMath::FRandRange(0.f, 360.f);
		if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, Mat, FVector(3.f, Size, Size), Where, Rot, Life))
		{
			Decal->SetFadeScreenSize(0.0015f);
		}
	};

	if (Surface == DCWeaponP9::SurfaceWorld)
	{
		SpawnDecal(BulletHoleDecal, End, Normal, FMath::FRandRange(1.1f, 1.6f), 90.f);
		DCAudio::PlayAt(this, EDCSound::ImpactDirt, End, 0.8f);
		return;
	}

	// Flesh: wet hit, then blood on whatever is behind the target and on the ground below.
	DCAudio::PlayAt(this, EDCSound::ImpactFlesh, End);
	FCollisionObjectQueryParams Statics(ECC_WorldStatic);
	FHitResult Behind;
	if (World->LineTraceSingleByObjectType(Behind, End, End + ShotDir * 220.f, Statics))
	{
		SpawnDecal(BloodDecal, Behind.ImpactPoint, Behind.ImpactNormal, FMath::FRandRange(12.f, 22.f), 120.f);
	}
	FHitResult Ground;
	if (World->LineTraceSingleByObjectType(Ground, End, End - FVector(0.f, 0.f, 400.f), Statics))
	{
		SpawnDecal(BloodDecal, Ground.ImpactPoint + ShotDir * FMath::FRandRange(10.f, 60.f), Ground.ImpactNormal, FMath::FRandRange(14.f, 28.f), 120.f);
	}
}

// ---------------------------------------------------------------------------
// Reloading
// ---------------------------------------------------------------------------

void ADCWeapon::RequestReload()
{
	if (!bReloading && AmmoInMagazine < MagazineSize && GetReserveAmmo() > 0)
	{
		Server_Reload();
	}
}

void ADCWeapon::Server_Reload_Implementation()
{
	if (bReloading || AmmoInMagazine >= MagazineSize || GetReserveAmmo() <= 0)
	{
		return;
	}
	bReloading = true;
	OnRep_Reloading();
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ADCWeapon::FinishReload, ReloadSeconds, false);
}

void ADCWeapon::OnRep_Reloading()
{
	if (bReloading)
	{
		ReloadStartTime = GetWorld()->GetTimeSeconds();
	}
}

void ADCWeapon::FinishReload()
{
	bReloading = false;

	const int32 Needed = MagazineSize - AmmoInMagazine;
	if (!ResolvedAmmoItem)
	{
		AmmoInMagazine = MagazineSize;
		return;
	}

	ADCCharacter* Character = Cast<ADCCharacter>(GetOwner());
	if (Character)
	{
		AmmoInMagazine += Character->GetInventoryComponent()->RemoveItem(ResolvedAmmoItem, Needed);
	}
}

// ---------------------------------------------------------------------------
// Procedural action animation (cosmetic, runs everywhere)
// ---------------------------------------------------------------------------

void ADCWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateActionVisuals();
	}
}

void ADCWeapon::UpdateActionVisuals()
{
	using namespace DCWeaponP9;
	const float Now = GetWorld()->GetTimeSeconds();

	// Slide: snaps back over the first 35% of the cycle, returns over the rest.
	// Locks back on an empty magazine until the reload finishes.
	float SlideAlpha = 0.f;
	const float CycleT = (Now - GetLastShotTime()) / FMath::Max(0.01f, CycleSeconds);
	if (CycleT >= 0.f && CycleT < 1.f)
	{
		SlideAlpha = CycleT < 0.35f ? CycleT / 0.35f : 1.f - (CycleT - 0.35f) / 0.65f;
	}
	const bool bLockedBack = AmmoInMagazine <= 0 && !bReloading && LastFireVisualTime > 0.f;
	if (bLockedBack && CycleT >= 0.35f)
	{
		SlideAlpha = 1.f;
	}
	SlideAlpha = FMath::Clamp(SlideAlpha, 0.f, 1.f);
	const float SlideX = -SlideTravelCm * SlideAlpha;
	SlideMesh->SetRelativeLocation(FVector(SlideX, 0.f, 0.f));

	// Barrel: rides back ~3mm with the slide while locked, then tilts its muzzle up to unlock.
	const float BarrelBack = FMath::Max(SlideX, -0.3f);
	const float Unlock = FMath::Clamp((SlideAlpha - 0.12f) / 0.4f, 0.f, 1.f);
	BarrelMesh->SetRelativeTransform(RotateAbout(BarrelLugPivot, FRotator(BarrelTiltDegrees * Unlock, 0.f, 0.f), FVector(BarrelBack, 0.f, 0.f)));

	// Recoil spring compresses toward its rear seat.
	const float Scale = (RecoilSpringLength + SlideX) / RecoilSpringLength;
	RecoilSpringMesh->SetRelativeScale3D(FVector(Scale, 1.f, 1.f));
	RecoilSpringMesh->SetRelativeLocation(FVector(RecoilSpringRearX * (1.f - Scale), 0.f, 0.f));

	// Trigger: pulled while held locally, or briefly on each shot for remote viewers.
	const bool bPulled = bTriggerHeld || (CycleT >= 0.f && CycleT < 1.f);
	TriggerMesh->SetRelativeTransform(RotateAbout(TriggerPivot, FRotator(bPulled ? -TriggerPullDegrees : 0.f, 0.f, 0.f)));

	// Magazine: drops out, gone, then seats again over the reload.
	float MagDrop = 0.f;
	bool bMagVisible = true;
	if (bReloading)
	{
		const float R = (Now - ReloadStartTime) / FMath::Max(0.01f, ReloadSeconds);
		if (R < 0.3f)
		{
			MagDrop = FMath::InterpEaseIn(0.f, MagDropCm, R / 0.3f, 2.f);
		}
		else if (R < 0.65f)
		{
			bMagVisible = false;
		}
		else
		{
			MagDrop = FMath::InterpEaseOut(MagDropCm, 0.f, FMath::Clamp((R - 0.65f) / 0.3f, 0.f, 1.f), 2.f);
		}
	}
	// Drop along the magazine's own axis (20.5 degrees of grip rake).
	const FVector MagAxis(FMath::Sin(FMath::DegreesToRadians(20.5f)), 0.f, FMath::Cos(FMath::DegreesToRadians(20.5f)));
	MagazineMesh->SetRelativeLocation(-MagAxis * MagDrop);
	MagazineMesh->SetVisibility(bMagVisible, true);

	// Top rounds reflect what's left in the magazine.
	for (int32 i = 0; i < RoundMeshes.Num(); ++i)
	{
		RoundMeshes[i]->SetVisibility(bMagVisible && (bReloading || AmmoInMagazine > i));
	}

	// Muzzle flash: a couple of frames of light and fire, randomly rotated each shot.
	const float FlashT = Now - GetLastShotTime();
	const bool bFlash = FlashT >= 0.f && FlashT < FlashSeconds;
	if (bFlash && !FlashFront->IsVisible())
	{
		const float Spin = FMath::FRandRange(0.f, 360.f);
		FlashFront->SetRelativeRotation(FRotator(90.f, Spin, 0.f));
		FlashSide->SetRelativeRotation(FRotator(0.f, 0.f, 90.f + FMath::FRandRange(-20.f, 20.f)));
		const float Size = FMath::FRandRange(0.85f, 1.2f);
		FlashFront->SetRelativeScale3D(FVector(0.07f * Size));
		FlashSide->SetRelativeScale3D(FVector(0.13f * Size, 0.06f * Size, 1.f));
	}
	FlashFront->SetVisibility(bFlash);
	FlashSide->SetVisibility(bFlash);
	const float LightT = FlashT / 0.07f;
	const bool bLight = FlashT >= 0.f && LightT < 1.f;
	MuzzleLight->SetVisibility(bLight);
	MuzzleLight->SetIntensity(bLight ? 9000.f * (1.f - LightT) : 0.f);

	// Reload sounds keyed to the magazine animation; slide release only if it was locked back.
	if (bReloading)
	{
		if (!bReloadVisualActive)
		{
			bReloadVisualActive = true;
			bPlayedMagOut = bPlayedMagIn = false;
			bSlideLockedAtReloadStart = AmmoInMagazine <= 0;
		}
		const float R = (Now - ReloadStartTime) / FMath::Max(0.01f, ReloadSeconds);
		if (!bPlayedMagOut && R > 0.02f)
		{
			bPlayedMagOut = true;
			DCAudio::PlayAt(this, EDCSound::MagOut, GetActorLocation());
		}
		if (!bPlayedMagIn && R > 0.9f)
		{
			bPlayedMagIn = true;
			DCAudio::PlayAt(this, EDCSound::MagIn, GetActorLocation());
		}
	}
	else if (bReloadVisualActive)
	{
		bReloadVisualActive = false;
		if (bSlideLockedAtReloadStart)
		{
			DCAudio::PlayAt(this, EDCSound::SlideRelease, GetActorLocation());
		}
	}
}
