#include "Player/DCCharacter.h"
#include "DeadCountry.h"
#include "Player/DCPlayerController.h"
#include "Health/DCHealthComponent.h"
#include "Health/DCSurvivalComponent.h"
#include "Inventory/DCInventoryComponent.h"
#include "Inventory/DCItemDefinition.h"
#include "Interaction/DCInteractable.h"
#include "Weapons/DCWeapon.h"
#include "Weapons/DCEquippable.h"
#include "Survival/DCSkillsComponent.h"
#include "World/DCWorldItem.h"
#include "World/DCLootContainer.h"
#include "Core/DCAudio.h"
#include "Survival/DCSurvivalGameMode.h"
#include "Survival/DCPlayerState.h"
#include "Survival/DCWorldSave.h"
#include "AI/DCZombie.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "DCCharacter"

ADCCharacter::ADCCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(85.f);

	// Epic mannequin placeholder art (swap for licensed survivor art later).
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> PistolAim(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> PistolReload(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> PistolWalk(TEXT("/Game/Characters/Mannequins/Anims/Pistol/Walk/MF_Pistol_Walk_Fwd.MF_Pistol_Walk_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> PistolJog(TEXT("/Game/Characters/Mannequins/Anims/Pistol/Jog/MF_Pistol_Jog_Fwd.MF_Pistol_Jog_Fwd"));
	ArmsIdleAnim = PistolAim.Object;
	ArmsReloadAnim = PistolReload.Object;
	BodyIdleAnim = PistolAim.Object;
	BodyWalkAnim = PistolWalk.Object;
	BodyJogAnim = PistolJog.Object;

	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FirstPersonCamera);
	FirstPersonArms->SetSkeletalMeshAsset(Manny.Object);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->SetCastShadow(false);
	FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonArms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	FirstPersonArms->bOnlyOwnerSee = true;
	FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// Full body for everyone else; the owner sees only the arms.
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMeshAsset(Manny.Object);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -89.f));
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetOwnerNoSee(true);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Health = CreateDefaultSubobject<UDCHealthComponent>(TEXT("Health"));
	Survival = CreateDefaultSubobject<UDCSurvivalComponent>(TEXT("Survival"));
	Inventory = CreateDefaultSubobject<UDCInventoryComponent>(TEXT("Inventory"));
	Skills = CreateDefaultSubobject<UDCSkillsComponent>(TEXT("Skills"));

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->JumpZVelocity = 380.f;
	Move->AirControl = 0.15f;

	// Starting kit: a cricket bat in hand (melee first - guns are loud), the P9 and a little ammo in the bag.
	StartingEquippedItem = TSoftObjectPtr<UDCItemDefinition>(FSoftObjectPath(TEXT("/Game/Items/DA_CricketBat.DA_CricketBat")));

	auto AddStarting = [this](const TCHAR* Path, int32 Count)
	{
		FDCStartingItem& Entry = StartingItems.AddDefaulted_GetRef();
		Entry.Item = TSoftObjectPtr<UDCItemDefinition>(FSoftObjectPath(Path));
		Entry.Count = Count;
	};
	AddStarting(TEXT("/Game/Items/DA_WaterBottle.DA_WaterBottle"), 1);
	AddStarting(TEXT("/Game/Items/DA_Bandage.DA_Bandage"), 2);
	AddStarting(TEXT("/Game/Items/DA_Ammo9mm.DA_Ammo9mm"), 17);
	AddStarting(TEXT("/Game/Items/DA_CricketBat.DA_CricketBat"), 1);
	AddStarting(TEXT("/Game/Items/DA_P9.DA_P9"), 1);
}

void ADCCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADCCharacter, EquippedWeapon);
	DOREPLIFETIME_CONDITION(ADCCharacter, bIsSprinting, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ADCCharacter, bIsAiming, COND_SkipOwner);
	DOREPLIFETIME(ADCCharacter, bSleeping);
}

void ADCCharacter::BeginPlay()
{
	Super::BeginPlay();

	// First-person arms: no head (it would sit over the camera) and no legs.
	FirstPersonArms->HideBoneByName(TEXT("neck_01"), PBO_None);
	FirstPersonArms->HideBoneByName(TEXT("thigh_l"), PBO_None);
	FirstPersonArms->HideBoneByName(TEXT("thigh_r"), PBO_None);
	FirstPersonArms->SetVisibility(IsLocallyControlled());

	Health->OnHealthChanged.AddDynamic(this, &ADCCharacter::HandleStatusChanged);
	Inventory->OnInventoryChanged.AddDynamic(this, &ADCCharacter::HandleStatusChanged);

	if (HasAuthority())
	{
		Health->OnDeath.AddDynamic(this, &ADCCharacter::HandleDeath);
		GiveStartingItems();
		EquipStartingItem();
	}
	UpdateMovementSpeed();
}

float ADCCharacter::GetOverweightThresholdKg() const
{
	return OverweightKg + 2.f * (Skills ? Skills->GetLevel(EDCSkill::Strength) : 0);
}

void ADCCharacter::Destroyed()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
	}
	Super::Destroyed();
}

bool ADCCharacter::IsDead() const
{
	return Health && Health->IsDead();
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void ADCCharacter::EquipStartingItem()
{
	if (const UDCItemDefinition* Item = StartingEquippedItem.LoadSynchronous())
	{
		if (const FDCInventoryEntry* Entry = Inventory->FindFirstOf(Item))
		{
			EquipEntry(Entry->EntryId);
		}
	}
}

void ADCCharacter::EquipEntry(int32 EntryId)
{
	if (!HasAuthority() || IsDead())
	{
		return;
	}
	if (EquippedWeapon && EquippedWeapon->GetEntryId() == EntryId)
	{
		Unequip(); // same item again = put it away
		return;
	}
	const FDCInventoryEntry* Entry = Inventory->FindEntry(EntryId);
	if (!Entry || !Entry->Item || !Entry->Item->EquipClass)
	{
		return;
	}
	const FDCInventoryEntry EntryCopy = *Entry; // Unequip may modify the array
	Unequip();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	ADCEquippable* NewItem = GetWorld()->SpawnActor<ADCEquippable>(EntryCopy.Item->EquipClass, GetActorTransform(), Params);
	if (!NewItem)
	{
		return;
	}
	NewItem->InitFromEntry(EntryCopy);
	EquippedWeapon = NewItem;
	bIsAiming = false;
	AttachWeapon();
	UpdateMovementSpeed();
}

void ADCCharacter::Unequip()
{
	if (!HasAuthority() || !EquippedWeapon)
	{
		return;
	}
	float Condition = -1.f;
	int32 Ammo = -1;
	EquippedWeapon->StopPrimary();
	EquippedWeapon->GetInstanceData(Condition, Ammo);
	Inventory->SetEntryInstanceData(EquippedWeapon->GetEntryId(), Condition, Ammo);
	EquippedWeapon->Destroy();
	EquippedWeapon = nullptr;
	bIsAiming = false;
	UpdateMovementSpeed();
}

void ADCCharacter::HandleEquippedBroken()
{
	if (!HasAuthority() || !EquippedWeapon)
	{
		return;
	}
	const FText Name = EquippedWeapon->GetDisplayName();
	const int32 EntryId = EquippedWeapon->GetEntryId();
	EquippedWeapon->Destroy();
	EquippedWeapon = nullptr;
	Inventory->RemoveFromEntry(EntryId, 1);
	Multicast_WeaponBroke();
	SendMessageToOwner(FText::Format(LOCTEXT("Broke", "Your {0} broke!"), Name));
}

void ADCCharacter::Multicast_WeaponBroke_Implementation()
{
	DCAudio::PlayAt(this, EDCSound::WeaponBreak, GetActorLocation() + FVector(0.f, 0.f, 40.f));
}

void ADCCharacter::GiveStartingItems()
{
	for (const FDCStartingItem& Starting : StartingItems)
	{
		if (UDCItemDefinition* Item = Starting.Item.LoadSynchronous())
		{
			Inventory->AddItem(Item, Starting.Count);
		}
	}
}

void ADCCharacter::AttachWeapon()
{
	if (EquippedWeapon)
	{
		// The equippable supplies its own held pose every frame (UpdateAimVisuals).
		EquippedWeapon->AttachToComponent(FirstPersonCamera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		CurrentArmsAnim = nullptr;
	}
}

void ADCCharacter::OnRep_EquippedWeapon()
{
	AttachWeapon();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void ADCCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	ADCPlayerController* PC = Cast<ADCPlayerController>(GetController());
	if (!Input || !PC)
	{
		UE_LOG(LogDC, Error, TEXT("DCCharacter needs an EnhancedInputComponent and a DCPlayerController."));
		return;
	}

	const FDCInputActions& A = PC->GetInputActions();
	Input->BindAction(A.Move, ETriggerEvent::Triggered, this, &ADCCharacter::Input_Move);
	Input->BindAction(A.Look, ETriggerEvent::Triggered, this, &ADCCharacter::Input_Look);
	Input->BindAction(A.Jump, ETriggerEvent::Started, this, &ADCCharacter::Input_JumpStarted);
	Input->BindAction(A.Jump, ETriggerEvent::Completed, this, &ADCCharacter::Input_JumpStopped);
	Input->BindAction(A.Crouch, ETriggerEvent::Started, this, &ADCCharacter::Input_ToggleCrouch);
	Input->BindAction(A.Sprint, ETriggerEvent::Started, this, &ADCCharacter::Input_SprintStarted);
	Input->BindAction(A.Sprint, ETriggerEvent::Completed, this, &ADCCharacter::Input_SprintStopped);
	Input->BindAction(A.Interact, ETriggerEvent::Started, this, &ADCCharacter::Input_Interact);
	Input->BindAction(A.Fire, ETriggerEvent::Started, this, &ADCCharacter::Input_FireStarted);
	Input->BindAction(A.Fire, ETriggerEvent::Completed, this, &ADCCharacter::Input_FireStopped);
	Input->BindAction(A.Reload, ETriggerEvent::Started, this, &ADCCharacter::Input_Reload);
	Input->BindAction(A.Aim, ETriggerEvent::Started, this, &ADCCharacter::Input_AimStarted);
	Input->BindAction(A.Aim, ETriggerEvent::Completed, this, &ADCCharacter::Input_AimStopped);
	Input->BindAction(A.Sleep, ETriggerEvent::Started, this, &ADCCharacter::Input_ToggleSleep);
}

void ADCCharacter::Input_Move(const FInputActionValue& Value)
{
	if (bSleeping)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void ADCCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>() * FMath::Lerp(1.f, AimLookSensitivity, AimAlpha);
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void ADCCharacter::Input_JumpStarted() { if (!bSleeping) { Jump(); } }
void ADCCharacter::Input_JumpStopped() { StopJumping(); }

void ADCCharacter::Input_ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Input_SprintStopped();
		Crouch();
	}
}

void ADCCharacter::Input_SprintStarted()
{
	if (bSleeping)
	{
		return;
	}
	if (bIsCrouched)
	{
		UnCrouch();
	}
	if (bIsAiming)
	{
		SetAiming(false); // Sprinting drops you out of ADS.
	}
	bIsSprinting = true;
	UpdateMovementSpeed();
	Server_SetSprinting(true);
}

void ADCCharacter::Input_SprintStopped()
{
	bIsSprinting = false;
	UpdateMovementSpeed();
	Server_SetSprinting(false);
}

void ADCCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	UpdateMovementSpeed();
}

void ADCCharacter::OnRep_Sprinting()
{
	UpdateMovementSpeed();
}

void ADCCharacter::Input_Interact()
{
	if (AActor* Target = FocusedActor.Get())
	{
		Server_Interact(Target);
	}
}

void ADCCharacter::Server_Interact_Implementation(AActor* Target)
{
	if (IsDead() || !Target || !Target->Implements<UDCInteractable>())
	{
		return;
	}
	// Validate range on the server; allow some slack for latency.
	if (FVector::Dist(Target->GetActorLocation(), GetActorLocation()) > InteractRange + 150.f)
	{
		return;
	}
	if (IDCInteractable::Execute_CanInteract(Target, this))
	{
		IDCInteractable::Execute_Interact(Target, this);
	}
}

void ADCCharacter::Input_FireStarted()
{
	if (IsDead() || bSleeping)
	{
		return;
	}
	if (EquippedWeapon)
	{
		EquippedWeapon->StartPrimary();
	}
	else
	{
		Input_Shove(); // bare hands: shove
	}
}

void ADCCharacter::Input_FireStopped()
{
	if (EquippedWeapon)
	{
		EquippedWeapon->StopPrimary();
	}
}

// ---------------------------------------------------------------------------
// Shove / stomp
// ---------------------------------------------------------------------------

void ADCCharacter::Input_Shove()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (IsDead() || bSleeping || Now - ShoveStartTime < 0.6f || (EquippedWeapon && EquippedWeapon->IsBusy()))
	{
		return;
	}
	ShoveStartTime = Now;
	Server_Shove();
}

void ADCCharacter::Server_Shove_Implementation()
{
	if (IsDead() || bSleeping || Survival->GetStamina() < 2.f)
	{
		return;
	}
	Survival->ConsumeStamina(ShoveStaminaCost);

	const FVector Forward = GetControlRotation().Vector().GetSafeNormal2D();
	const FVector Origin = GetActorLocation();
	const int32 Strength = Skills->GetLevel(EDCSkill::Strength);

	// A downed zombie right in front gets stomped instead (Zomboid).
	ADCZombie* StompTarget = nullptr;
	TArray<ADCZombie*> ShoveTargets;
	for (TActorIterator<ADCZombie> It(GetWorld()); It; ++It)
	{
		ADCZombie* Zombie = *It;
		if (Zombie->IsDead())
		{
			continue;
		}
		const FVector To = Zombie->GetActorLocation() - Origin;
		const float Dist = To.Size2D();
		if (Dist > ShoveRange + 40.f || FVector::DotProduct(To.GetSafeNormal2D(), Forward) < 0.45f)
		{
			continue;
		}
		if (Zombie->IsKnockedDown())
		{
			if (!StompTarget && Dist < ShoveRange)
			{
				StompTarget = Zombie;
			}
		}
		else if (ShoveTargets.Num() < 3)
		{
			ShoveTargets.Add(Zombie);
		}
	}

	if (StompTarget)
	{
		FHitResult Hit;
		Hit.BoneName = TEXT("head");
		Hit.ImpactPoint = StompTarget->GetMesh()->GetSocketLocation(TEXT("head"));
		StompTarget->ApplyMeleeHit(StompDamage * (1.f + 0.05f * Strength), FVector::DownVector, Hit, false, GetController());
		Skills->AddXP(EDCSkill::Strength, 2.f);
		Multicast_ShoveFX(true);
		return;
	}

	for (ADCZombie* Zombie : ShoveTargets)
	{
		const FVector Dir = (Zombie->GetActorLocation() - Origin).GetSafeNormal2D();
		Zombie->Shove(Dir, ShoveKnockdownChance + 0.03f * Strength, GetController());
	}
	if (ShoveTargets.Num() > 0)
	{
		Skills->AddXP(EDCSkill::Strength, 1.f);
	}
	Multicast_ShoveFX(false);
}

void ADCCharacter::Multicast_ShoveFX_Implementation(bool bStomp)
{
	if (!IsLocallyControlled())
	{
		ShoveStartTime = GetWorld()->GetTimeSeconds();
	}
	DCAudio::PlayAt(this, bStomp ? EDCSound::MeleeHitBlunt : EDCSound::Shove, GetActorLocation(), bStomp ? 1.f : 0.8f);
}

void ADCCharacter::Input_Reload()
{
	if (EquippedWeapon && !IsDead() && !bSleeping)
	{
		EquippedWeapon->RequestReload();
	}
}

// ---------------------------------------------------------------------------
// Aim down sights
// ---------------------------------------------------------------------------

void ADCCharacter::Input_AimStarted()
{
	// Guns aim down sights; melee weapons and empty hands shove.
	if (EquippedWeapon && EquippedWeapon->CanAim())
	{
		SetAiming(true);
	}
	else
	{
		Input_Shove();
	}
}

void ADCCharacter::Input_AimStopped()
{
	SetAiming(false);
}

void ADCCharacter::SetAiming(bool bNewAiming)
{
	if (bNewAiming && (IsDead() || !EquippedWeapon || !EquippedWeapon->CanAim() || bSleeping))
	{
		return;
	}
	if (bNewAiming && bIsSprinting)
	{
		Input_SprintStopped();
	}
	if (bIsAiming == bNewAiming)
	{
		return;
	}
	bIsAiming = bNewAiming;
	UpdateMovementSpeed();
	if (!HasAuthority())
	{
		Server_SetAiming(bNewAiming);
	}
}

void ADCCharacter::Server_SetAiming_Implementation(bool bNewAiming)
{
	bIsAiming = bNewAiming && !IsDead();
	if (bIsAiming)
	{
		bIsSprinting = false;
	}
	UpdateMovementSpeed();
}

void ADCCharacter::OnRep_Aiming()
{
	UpdateMovementSpeed();
}

FName ADCCharacter::GetHandAnchorName(const USkeletalMeshComponent* SkelMesh) const
{
	static const FName GripSocket(TEXT("HandGrip_R"));
	static const FName HandBone(TEXT("hand_r"));
	return (SkelMesh && SkelMesh->DoesSocketExist(GripSocket)) ? GripSocket : HandBone;
}

void ADCCharacter::UpdateFirstPersonArms()
{
	if (!EquippedWeapon || !FirstPersonArms->GetSkinnedAsset())
	{
		FirstPersonArms->SetVisibility(false);
		return;
	}
	FirstPersonArms->SetVisibility(!bSleeping);

	// Pose: aiming idle, swapping to the reload clip (stretched to the weapon's reload time).
	UAnimSequence* Want = (EquippedWeapon->WantsReloadArmsPose() && ArmsReloadAnim) ? ArmsReloadAnim : ArmsIdleAnim;
	if (Want && Want != CurrentArmsAnim)
	{
		const bool bLoop = Want != ArmsReloadAnim;
		FirstPersonArms->PlayAnimation(Want, bLoop);
		if (!bLoop)
		{
			FirstPersonArms->SetPlayRate(Want->GetPlayLength() / FMath::Max(0.1f, EquippedWeapon->GetReloadDuration()));
		}
		else
		{
			FirstPersonArms->SetPlayRate(1.f);
		}
		CurrentArmsAnim = Want;
	}

	// Place the whole arm mesh so its right hand lands on the grip, oriented with the gun.
	// Orientation comes from the gun itself (FBX sockets carry an axis-conversion rotation); the socket gives position.
	const FTransform Grip = EquippedWeapon->GetGripTransform();
	const FQuat MeshRot = EquippedWeapon->GetArmsReferenceRotation() * ArmsRotationOffset.Quaternion();
	const FName Anchor = GetHandAnchorName(FirstPersonArms);
	const FVector HandInMesh = FirstPersonArms->GetSocketTransform(Anchor, RTS_Component).GetLocation() + ArmsGripOffset;
	FirstPersonArms->SetWorldLocationAndRotation(Grip.GetLocation() - MeshRot.RotateVector(HandInMesh), MeshRot);
}

void ADCCharacter::LogArmsDebug() const
{
	const USkeletalMeshComponent* Arms = FirstPersonArms;
	UE_LOG(LogDC, Log, TEXT("[ArmsDebug] anim=%s playing=%d mode=%d anchor=%s"),
		CurrentArmsAnim ? *CurrentArmsAnim->GetName() : TEXT("none"), Arms->IsPlaying() ? 1 : 0,
		static_cast<int32>(Arms->GetAnimationMode()), *GetHandAnchorName(Arms).ToString());
	for (const TCHAR* Bone : { TEXT("pelvis"), TEXT("spine_05"), TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"),
		TEXT("hand_r"), TEXT("hand_l"), TEXT("HandGrip_R") })
	{
		const bool bSocket = Arms->DoesSocketExist(Bone);
		const FTransform T = Arms->GetSocketTransform(Bone, RTS_Component);
		UE_LOG(LogDC, Log, TEXT("[ArmsDebug]   %-12s exists=%d comp=%s scale=%s"), Bone, bSocket ? 1 : 0,
			*T.GetLocation().ToCompactString(), *T.GetScale3D().ToCompactString());
	}
	UE_LOG(LogDC, Log, TEXT("[ArmsDebug] world=%s rot=%s camera=%s"), *Arms->GetComponentLocation().ToCompactString(),
		*Arms->GetComponentRotation().ToCompactString(), *FirstPersonCamera->GetComponentLocation().ToCompactString());
	UE_LOG(LogDC, Log, TEXT("[ArmsDebug] aiming=%d alpha=%.2f weaponRel=%s fov=%.1f"), bIsAiming ? 1 : 0, AimAlpha,
		EquippedWeapon ? *EquippedWeapon->GetRootComponent()->GetRelativeLocation().ToCompactString() : TEXT("-"),
		FirstPersonCamera->FieldOfView);
}

void ADCCharacter::UpdateThirdPersonBody()
{
	USkeletalMeshComponent* Body = GetMesh();
	const float Speed = GetVelocity().Size2D();
	UAnimSequence* Want = BodyIdleAnim;
	float Rate = 1.f;
	if (Speed > 400.f && BodyJogAnim)
	{
		Want = BodyJogAnim;
		Rate = FMath::Clamp(Speed / 420.f, 0.6f, 1.4f);
	}
	else if (Speed > 20.f && BodyWalkAnim)
	{
		Want = BodyWalkAnim;
		Rate = FMath::Clamp(Speed / 170.f, 0.5f, 1.6f);
	}
	if (Want && Want != CurrentBodyAnim)
	{
		Body->PlayAnimation(Want, true);
		CurrentBodyAnim = Want;
	}
	Body->SetPlayRate(Rate);

	// Put the gun in the body's right hand (the mirror of the first-person solve).
	if (EquippedWeapon)
	{
		const FTransform Hand = Body->GetSocketTransform(GetHandAnchorName(Body));
		const FQuat GunRot = Body->GetComponentQuat() * ArmsRotationOffset.Quaternion().Inverse();
		const FTransform GripLocal = EquippedWeapon->GetGripLocalTransform();
		EquippedWeapon->GetRootComponent()->SetWorldLocationAndRotation(Hand.GetLocation() - GunRot.RotateVector(GripLocal.GetLocation()), GunRot);
	}
}

void ADCCharacter::UpdateFootstepAudio(float DeltaSeconds)
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();
	if (bSleeping || IsDead() || !Move->IsMovingOnGround() || Speed < 40.f)
	{
		FootstepDistance = 0.f;
		return;
	}
	FootstepDistance += Speed * DeltaSeconds;
	const float Stride = bIsCrouched ? 75.f : (bIsSprinting ? 175.f : 135.f);
	if (FootstepDistance >= Stride)
	{
		FootstepDistance = 0.f;
		const float Volume = bIsCrouched ? 0.18f : (bIsSprinting ? 1.f : 0.55f);
		const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		DCAudio::PlayAt(this, EDCSound::Footstep, Feet, Volume);
	}
}

void ADCCharacter::ApplyRecoil(float PitchKick, float YawKick, float RecoverFraction)
{
	AController* MyController = GetController();
	if (!IsLocallyControlled() || !MyController)
	{
		return;
	}
	FRotator Rotation = MyController->GetControlRotation();
	Rotation.Pitch += PitchKick;
	Rotation.Yaw += YawKick;
	MyController->SetControlRotation(Rotation);
	PendingRecoilRecovery += PitchKick * FMath::Clamp(RecoverFraction, 0.f, 1.f);
}

void ADCCharacter::UpdateRecoilRecovery(float DeltaSeconds)
{
	AController* MyController = GetController();
	if (PendingRecoilRecovery <= 0.f || !MyController)
	{
		return;
	}
	// Ease out: fast at first, slowing as it settles.
	const float Step = FMath::Min(PendingRecoilRecovery, FMath::Max(RecoilRecoverySpeed * DeltaSeconds, PendingRecoilRecovery * 10.f * DeltaSeconds));
	FRotator Rotation = MyController->GetControlRotation();
	Rotation.Pitch -= Step;
	MyController->SetControlRotation(Rotation);
	PendingRecoilRecovery -= Step;
}

void ADCCharacter::UpdateAimVisuals(float DeltaSeconds)
{
	AimAlpha = FMath::FInterpTo(AimAlpha, bIsAiming ? 1.f : 0.f, DeltaSeconds, AimInterpSpeed);
	const float T = FMath::SmoothStep(0.f, 1.f, AimAlpha);

	if (EquippedWeapon)
	{
		// The equippable owns its pose (hip/ADS + recoil, or a melee swing)...
		FVector Location;
		FRotator Rotation;
		EquippedWeapon->GetHeldPose(AimAlpha, Location, Rotation);

		// ...and a shove thrusts whatever is in hand forward.
		const float ShoveT = (GetWorld()->GetTimeSeconds() - ShoveStartTime) / 0.35f;
		if (ShoveT >= 0.f && ShoveT < 1.f)
		{
			const float Push = FMath::Sin(ShoveT * PI);
			Location += FVector(16.f * Push, -6.f * Push, 4.f * Push);
			Rotation += FRotator(-25.f * Push, 0.f, 0.f);
		}
		EquippedWeapon->GetRootComponent()->SetRelativeLocationAndRotation(Location, Rotation);
	}
	if (IsLocallyControlled())
	{
		FirstPersonCamera->SetFieldOfView(FMath::Lerp(HipFieldOfView, AimFieldOfView, T));
	}
}

// ---------------------------------------------------------------------------
// Tick: focus trace (local), noise + sprint upkeep (server)
// ---------------------------------------------------------------------------

void ADCCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
	{
		UpdateFocus();

		const ADCPlayerController* PC = Cast<ADCPlayerController>(GetController());
		if (PC && PC->bDebugForceAim && !bIsAiming)
		{
			SetAiming(true);
			AimAlpha = 1.f; // snap, so screenshots show the final sight picture
		}
		UpdateRecoilRecovery(DeltaSeconds);
	}
	UpdateAimVisuals(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (IsLocallyControlled())
		{
			UpdateFirstPersonArms();
		}
		else
		{
			UpdateThirdPersonBody();
		}
	}
	UpdateFootstepAudio(DeltaSeconds);

	if (HasAuthority() && !IsDead())
	{
		if (bSleeping)
		{
			UpdateSleep(DeltaSeconds);
		}

		if (bIsSprinting && !CanSprint())
		{
			bIsSprinting = false;
			UpdateMovementSpeed();
		}

		// Footsteps make noise. Sprinting is loud, walking is quiet, crouching is silent.
		const float Speed = GetVelocity().Size2D();
		if (Speed > 50.f && !bIsCrouched && GetCharacterMovement()->IsMovingOnGround())
		{
			FootstepNoiseTimer -= DeltaSeconds;
			if (FootstepNoiseTimer <= 0.f)
			{
				FootstepNoiseTimer = 0.5f;
				// Sneaking skill: 6% quieter per level.
				const float Sneak = 1.f - 0.06f * Skills->GetLevel(EDCSkill::Sneaking);
				const float Range = (bIsSprinting ? 1800.f : 600.f) * Sneak;
				MakeNoise(bIsSprinting ? 0.6f : 0.25f, this, GetActorLocation(), Range);
			}
		}
		UpdateSkillTraining(DeltaSeconds);
	}
}

void ADCCharacter::UpdateSkillTraining(float DeltaSeconds)
{
	SkillTrainingAccumulator += DeltaSeconds;
	if (SkillTrainingAccumulator < 1.f)
	{
		return;
	}
	const float Seconds = SkillTrainingAccumulator;
	SkillTrainingAccumulator = 0.f;

	const float Speed = GetVelocity().Size2D();
	if (Speed < 50.f || !GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	if (bIsSprinting)
	{
		Skills->AddXP(EDCSkill::Fitness, 0.6f * Seconds);
	}
	if (bIsCrouched)
	{
		Skills->AddXP(EDCSkill::Sneaking, (IsZombieNearby(2000.f) ? 0.6f : 0.15f) * Seconds);
	}
	if (Inventory->GetTotalWeight() > OverweightKg * 0.75f)
	{
		Skills->AddXP(EDCSkill::Strength, 0.1f * Seconds);
	}
}

void ADCCharacter::UpdateFocus()
{
	FocusedActor.Reset();
	if (IsDead())
	{
		return;
	}

	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + FirstPersonCamera->GetForwardVector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DCInteractTrace), false, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor->Implements<UDCInteractable>())
		{
			FocusedActor = HitActor;
		}
	}
}

FText ADCCharacter::GetFocusedInteractionText() const
{
	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return FText::GetEmpty();
	}
	return IDCInteractable::Execute_GetInteractionText(Target, const_cast<ADCCharacter*>(this));
}

// ---------------------------------------------------------------------------
// Movement speed
// ---------------------------------------------------------------------------

bool ADCCharacter::CanSprint() const
{
	return !bIsCrouched && !bIsAiming
		&& Survival && Survival->HasStaminaToSprint()
		&& Health && !Health->HasLegInjury();
}

void ADCCharacter::UpdateMovementSpeed()
{
	float Speed = (bIsSprinting && CanSprint()) ? SprintSpeed : WalkSpeed;
	if (Health && Health->HasLegInjury())
	{
		Speed *= InjuredLegSpeedMultiplier;
	}
	if (Inventory && Inventory->GetTotalWeight() > GetOverweightThresholdKg())
	{
		Speed *= OverweightSpeedMultiplier;
	}
	if (bIsAiming)
	{
		Speed *= AimMoveSpeedMultiplier;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void ADCCharacter::HandleStatusChanged()
{
	UpdateMovementSpeed();
}

// ---------------------------------------------------------------------------
// Damage / death
// ---------------------------------------------------------------------------

float ADCCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IsDead() || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	EDCBodyPart Part = EDCBodyPart::Thorax;
	float BleedChance = 0.f;
	float FractureChance = 0.f;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		Part = UDCHealthComponent::BodyPartFromCapsuleHit(this, Point.HitInfo.ImpactPoint);
		BleedChance = 0.35f;
		FractureChance = 0.15f;
	}

	Health->ApplyDamageToPart(Part, DamageAmount, EventInstigator, BleedChance, FractureChance, false);
	return DamageAmount;
}

void ADCCharacter::HandleDeath(AController* Killer)
{
	// Server only. Drop a lootable body with everything we carried (the weapon in hand keeps its ammo/condition).
	Unequip();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector BodyLocation = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 20.f);
	if (ADCLootContainer* Body = GetWorld()->SpawnActor<ADCLootContainer>(ADCLootContainer::StaticClass(), BodyLocation, GetActorRotation(), Params))
	{
		const ADCPlayerState* PS = GetPlayerState<ADCPlayerState>();
		Body->ContainerName = (PS && !PS->SurvivorName.IsEmpty())
			? FText::Format(LOCTEXT("NamedBody", "Body of {0}"), FText::FromString(PS->SurvivorName))
			: LOCTEXT("Body", "Body");
		Body->bDestroyWhenEmpty = true;
		Body->GetInventoryComponent()->GridWidth = Inventory->GridWidth;
		Body->GetInventoryComponent()->GridHeight = Inventory->GridHeight;
		Inventory->TransferAllTo(Body->GetInventoryComponent());
	}

	AController* MyController = GetController();
	if (ADCSurvivalGameMode* GM = GetWorld()->GetAuthGameMode<ADCSurvivalGameMode>())
	{
		GM->HandleSurvivorDied(MyController, Killer);
	}

	if (MyController)
	{
		MyController->UnPossess();
	}
	Destroy();
}

// ---------------------------------------------------------------------------
// Sleep
// ---------------------------------------------------------------------------

void ADCCharacter::Input_ToggleSleep()
{
	Server_ToggleSleep();
}

void ADCCharacter::Server_ToggleSleep_Implementation()
{
	if (IsDead())
	{
		return;
	}
	if (bSleeping)
	{
		StopSleeping();
		return;
	}
	if (!Survival->CanSleep())
	{
		SendMessageToOwner(LOCTEXT("NotTired", "You're not tired enough to sleep."));
		return;
	}
	if (IsZombieNearby(SleepZombieSafetyRadius))
	{
		SendMessageToOwner(LOCTEXT("ZombiesNear", "You can't sleep with zombies nearby."));
		return;
	}
	if (Health->IsBleeding())
	{
		SendMessageToOwner(LOCTEXT("Bleeding", "You can't sleep while you're bleeding."));
		return;
	}
	StartSleeping();
}

void ADCCharacter::UpdateSleep(float DeltaSeconds)
{
	SleepCheckTimer -= DeltaSeconds;
	if (SleepCheckTimer > 0.f)
	{
		return;
	}
	SleepCheckTimer = 0.5f;

	if (Health->GetLastAttackedTime() > SleepStartTime)
	{
		StopSleeping();
		SendMessageToOwner(LOCTEXT("WokenAttack", "You wake up to something tearing at you!"));
	}
	else if (IsZombieNearby(SleepWakeZombieRadius))
	{
		StopSleeping();
		SendMessageToOwner(LOCTEXT("WokenNoise", "A noise wakes you. Something is close..."));
	}
	else if (Survival->IsRested())
	{
		StopSleeping();
		SendMessageToOwner(LOCTEXT("WokenRested", "You wake up rested."));
	}
}

void ADCCharacter::StartSleeping()
{
	bSleeping = true;
	SleepStartTime = GetWorld()->GetTimeSeconds();
	bIsSprinting = false;
	bIsAiming = false;
	if (EquippedWeapon)
	{
		EquippedWeapon->StopPrimary();
	}
	ApplySleepMovementState();
	UpdateMovementSpeed();
}

void ADCCharacter::StopSleeping()
{
	bSleeping = false;
	ApplySleepMovementState();
}

void ADCCharacter::OnRep_Sleeping()
{
	ApplySleepMovementState();
	if (bSleeping)
	{
		bIsAiming = false;
		bIsSprinting = false;
	}
}

void ADCCharacter::ApplySleepMovementState()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bSleeping)
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	else if (Move->MovementMode == MOVE_None)
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

bool ADCCharacter::IsZombieNearby(float Radius) const
{
	const float RadiusSq = FMath::Square(Radius);
	for (TActorIterator<ADCZombie> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) < RadiusSq)
		{
			return true;
		}
	}
	return false;
}

void ADCCharacter::SendMessageToOwner(const FText& Message) const
{
	if (ADCPlayerController* PC = Cast<ADCPlayerController>(GetController()))
	{
		PC->Client_ShowMessage(Message);
	}
}

// ---------------------------------------------------------------------------
// World save
// ---------------------------------------------------------------------------

void ADCCharacter::WriteSaveData(FDCSavedCharacter& Out) const
{
	Out.Transform = GetActorTransform();
	Out.BodyParts = Health->GetBodyParts();
	Out.Infection = Health->GetInfection();
	Out.bInfected = Health->IsInfected();
	Out.Hydration = Survival->GetHydration();
	Out.Energy = Survival->GetEnergy();
	Out.Fatigue = Survival->GetFatigue();
	Inventory->ExportSave(Out.Inventory);

	// The item in hand holds the live ammo/condition; patch it into its saved entry.
	Out.bHasEquipped = false;
	if (EquippedWeapon)
	{
		if (const FDCInventoryEntry* Held = Inventory->FindEntry(EquippedWeapon->GetEntryId()))
		{
			Out.bHasEquipped = true;
			Out.EquippedPosition = Held->Position;
			for (FDCSavedInventoryEntry& Saved : Out.Inventory)
			{
				if (Saved.Position == Held->Position)
				{
					EquippedWeapon->GetInstanceData(Saved.Condition, Saved.LoadedAmmo);
				}
			}
		}
	}
	Out.SkillXP = Skills->GetAllXP();
}

void ADCCharacter::ReadSaveData(const FDCSavedCharacter& In)
{
	if (!HasAuthority())
	{
		return;
	}
	TeleportTo(In.Transform.GetLocation(), In.Transform.Rotator(), false, true);
	Health->RestoreState(In.BodyParts, In.Infection, In.bInfected);
	Survival->RestoreState(In.Hydration, In.Energy, In.Fatigue);
	// Drop the fresh starting kit (and what's in hand) and restore exactly what was saved.
	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
		EquippedWeapon = nullptr;
	}
	Inventory->ImportSave(In.Inventory);
	Skills->RestoreXP(In.SkillXP);
	if (In.bHasEquipped)
	{
		if (const FDCInventoryEntry* Held = Inventory->FindEntryAt(In.EquippedPosition))
		{
			EquipEntry(Held->EntryId);
		}
	}
	UE_LOG(LogDC, Log, TEXT("Restored survivor: %d items, in hand: %s (%s)"), Inventory->GetEntries().Num(),
		EquippedWeapon ? *EquippedWeapon->GetDisplayName().ToString() : TEXT("nothing"),
		EquippedWeapon ? *EquippedWeapon->GetHudPrimary() : TEXT("-"));
	UpdateMovementSpeed();
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------

void ADCCharacter::Server_UseItem_Implementation(int32 EntryId)
{
	const FDCInventoryEntry* Entry = Inventory->FindEntry(EntryId);
	if (IsDead() || !Entry || !Entry->Item)
	{
		return;
	}
	if (Entry->Item->EquipClass)
	{
		EquipEntry(EntryId); // weapons: take it in hand (or put it away)
		return;
	}
	if (!Entry->Item->bConsumable)
	{
		return;
	}

	const UDCItemDefinition* Item = Entry->Item;
	if (Item->HealAmount > 0.f)        Health->Heal(Item->HealAmount);
	if (Item->bStopsBleeding)          Health->StopOneBleed();
	if (Item->bSplintsFracture)        Health->SplintOneFracture();
	if (Item->HydrationRestore != 0.f) Survival->AddHydration(Item->HydrationRestore);
	if (Item->EnergyRestore != 0.f)    Survival->AddEnergy(Item->EnergyRestore);

	Inventory->RemoveFromEntry(EntryId, 1);
}

void ADCCharacter::Server_DropItem_Implementation(int32 EntryId)
{
	if (IsDead() || !Inventory->FindEntry(EntryId))
	{
		return;
	}
	if (EquippedWeapon && EquippedWeapon->GetEntryId() == EntryId)
	{
		Unequip(); // writes its ammo/condition back first
	}
	const FDCInventoryEntry Entry = *Inventory->FindEntry(EntryId);
	const FVector DropLocation = GetActorLocation() + GetActorForwardVector() * 80.f;
	if (Inventory->RemoveFromEntry(EntryId, Entry.Count))
	{
		ADCWorldItem::SpawnDropped(GetWorld(), Entry.Item, Entry.Count, DropLocation, FRotator(0.f, GetActorRotation().Yaw, 0.f),
			Entry.Condition, Entry.LoadedAmmo);
	}
}

#undef LOCTEXT_NAMESPACE
