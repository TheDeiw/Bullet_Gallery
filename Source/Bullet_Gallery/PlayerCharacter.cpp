#include "PlayerCharacter.h"

#include "Bullet_Gallery.h"
#include "GuardNPC.h"
#include "MuseumDirector.h"
#include "MuseumHUDWidget.h"
#include "MuseumUserSettings.h"
#include "MuseumProjectile.h"
#include "PlayerAnimInstance.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Sound/SoundBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

namespace
{
	// Layout of SM_StripperClip / SM_ClipRound (Lab6/Blender/make_stripper_clip.py): rounds stacked up from the bottom of the strip, cm
	constexpr int32 ClipCapacity = 10;
	constexpr float ClipFirstRoundZ = 0.75f;
	constexpr float ClipRoundPitch = 1.26f;
	constexpr float ClipRoundRadius = 0.62f;
	constexpr float ClipHeight = 13.6f;
	// How fast the thumb pushes the stack down, cm/s (in the model's units)
	constexpr float RoundSlideSpeed = 30.f;
	// The clip is drawn smaller than the model, so that it fits inside the pistol's magazine and can hide there
	constexpr float ClipScale = 0.6f;
	// Montage time spans: the clip rises out of the pistol (start of ReloadStart) and is pushed back in (start of ReloadEnd;
	// UPlayerAnimInstance::BlendOutTime starts when this ends)
	const FVector2f ClipRiseTime(0.f, 0.15f);
	const FVector2f ClipSinkTime(1.2f, 1.35f);
}

// Sets default values
APlayerCharacter::APlayerCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// The body turns with the mouse horizontally; only the camera pitches up and down
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// The mesh stands with its feet at the bottom of the capsule, facing forward
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight), FRotator(0.f, -90.f, 0.f));

	// Second body with the head (see BodyMesh in the header); set up in BeginPlay when the Blueprint's mesh is known
	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetMesh());
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetOwnerNoSee(true);
	BodyMesh->bCastHiddenShadow = true;

	// Camera at eye level, slightly in front of the head
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(15.f, 0.f, 70.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	// Flashlight: a warm spot light that follows the view; it does not light the player's own arms (they are lit by the world)
	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(FirstPersonCamera);
	// Not inverse-square: a close wall or enemy must not blow out (the arms and the weapon are only centimetres from the lens)
	Flashlight->SetRelativeLocation(FVector(30.f, 0.f, 0.f));
	Flashlight->bUseInverseSquaredFalloff = false;
	Flashlight->IntensityUnits = ELightUnits::Unitless;
	Flashlight->SetIntensity(4.f);
	Flashlight->SetLightFalloffExponent(2.f);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Flashlight->SetAttenuationRadius(2800.f);
	Flashlight->SetInnerConeAngle(8.f);
	Flashlight->SetOuterConeAngle(32.f);
	Flashlight->SetCastShadows(true);
	Flashlight->SetVisibility(true);

	// Weapon: created in the constructor so it is visible in the Blueprint viewport
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), WeaponSocketName);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Stripper clip for the reload, hidden until the left hand takes it; UpdateReloadClip places it every frame
	ClipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ClipMesh"));
	ClipMesh->SetupAttachment(WeaponMesh, ClipGuideSocketName);
	ClipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClipMesh->SetCastShadow(false);
	ClipRounds = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ClipRounds"));
	ClipRounds->SetupAttachment(ClipMesh);
	ClipRounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ClipRounds->SetCastShadow(false);

	// Warm flash at the muzzle, switched on for a moment on every shot
	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(WeaponMesh, MuzzleSocketName);
	MuzzleLight->IntensityUnits = ELightUnits::Candelas;
	MuzzleLight->Intensity = 150.f;
	MuzzleLight->AttenuationRadius = 400.f;
	MuzzleLight->LightColor = FColor(255, 170, 80);
	MuzzleLight->CastShadows = false;
	MuzzleLight->SetVisibility(false);

	// Carries light physics objects; soft springs so a carried vase does not jitter
	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));
	PhysicsHandle->LinearStiffness = 1500.f;
	PhysicsHandle->LinearDamping = 200.f;
	PhysicsHandle->AngularStiffness = 1500.f;
	PhysicsHandle->AngularDamping = 500.f;
	PhysicsHandle->InterpolationSpeed = 30.f;
}

// Called when the game starts or when spawned
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Two meshes: the first-person one has no head (the camera sits inside it) and is seen only by the owner;
	// the second one is a full copy with the head, hidden from the owner's camera but casting the shadow
	GetMesh()->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None);
	GetMesh()->SetOnlyOwnerSee(true);
	GetMesh()->SetCastShadow(false);

	BodyMesh->SetSkeletalMesh(GetMesh()->GetSkeletalMeshAsset());
	for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
	{
		BodyMesh->SetMaterial(i, GetMesh()->GetMaterial(i));
	}
	// Its own copy of the Animation Blueprint (a leader pose would copy the hidden head bone too)
	BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	BodyMesh->SetAnimInstanceClass(GetMesh()->GetAnimClass());

	// Every pass through the reload loop section reports "RoundsInserted"
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		Anim->OnPlayMontageNotifyBegin.AddDynamic(this, &APlayerCharacter::OnReloadNotify);
	}

	// Attach the weapon to the skeleton socket (also picks up a socket name overridden in the Blueprint)
	WeaponMesh->AttachToComponent(GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocketName);

	UpdateMovementSpeed();

	Health = FMath::Clamp(StartHealth, 1.f, MaxHealth);
	AmmoInMagazine = MagazineSize;
	ReserveAmmo = StartReserveAmmo;
	BaseMaxHealth = MaxHealth;
	BaseMaxReserveAmmo = MaxReserveAmmo;
	CameraBaseLocation = FirstPersonCamera->GetRelativeLocation();

	CreateHUDWidget();

	// The game director plays the music itself (it keeps playing in the menus); without it the player does
	if (MusicSound && !UMuseumDirector::Get(this))
	{
		UGameplayStatics::SpawnSound2D(this, MusicSound, MusicVolume);
	}
}

void APlayerCharacter::CreateHUDWidget()
{
	if (HUDWidget) { return; }

	// Only the local player sees a HUD
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || !PC->IsLocalController()) { return; }

	const TSubclassOf<UMuseumHUDWidget> WidgetClass = HUDWidgetClass ? HUDWidgetClass : TSubclassOf<UMuseumHUDWidget>(UMuseumHUDWidget::StaticClass());
	HUDWidget = CreateWidget<UMuseumHUDWidget>(PC, WidgetClass);
	if (!HUDWidget) { return; }

	HUDWidget->AddToViewport();

	// Show the starting values
	OnHealthChanged(Health, MaxHealth);
	OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);
}

// Called every frame
void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Smoothly zoom the camera in while aiming and back out when released
	const UMuseumUserSettings* Settings = UMuseumUserSettings::Get();
	const float BaseFOV = Settings ? Settings->FieldOfView : DefaultFOV;
	const float TargetFOV = (bIsAiming ? BaseFOV * (AimFOV / DefaultFOV) : BaseFOV) + ShotFovKick;
	FirstPersonCamera->SetFieldOfView(
		FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed));
	UpdateCameraFeel(DeltaTime);

	UpdateHeldObject();
	UpdateInteractionHint();

	// Footsteps: one step every JogStepDistance / WalkStepDistance travelled on the ground
	if (!bIsDead && GetCharacterMovement()->IsMovingOnGround() && FootstepSounds.Num() > 0)
	{
		StepTravelled += GetVelocity().Size2D() * DeltaTime;
		if (StepTravelled >= ((bWantsToWalk || bIsAiming) ? WalkStepDistance : JogStepDistance))
		{
			StepTravelled = 0.f;
			UGameplayStatics::PlaySound2D(this, FootstepSounds[FMath::RandHelper(FootstepSounds.Num())], 1.f, FMath::FRandRange(0.92f, 1.08f));
		}
	}
	else
	{
		StepTravelled = 0.f;
	}

	if (bDebugReload)
	{
		bDebugReload = false;
		AmmoInMagazine = 0;
		OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);
		Reload();
	}
	if (bDebugFire)
	{
		bDebugFire = false;
		Fire();
	}
	UpdateReloadClip(DeltaTime);
}

// Called when the controller possessing this character changes
void APlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Add the input mapping contexts once the player controller possesses this character
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext) { Subsystem->AddMappingContext(DefaultMappingContext, 0); }
			if (MouseLookMappingContext) { Subsystem->AddMappingContext(MouseLookMappingContext, 0); }
		}
	}

	// Possessed after BeginPlay: the HUD could not be created there
	if (HasActorBegunPlay())
	{
		CreateHUDWidget();
	}
}

// Called to bind functionality to input
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		Input->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerCharacter::Fire);
		Input->BindAction(WalkAction, ETriggerEvent::Started, this, &APlayerCharacter::StartWalk);
		Input->BindAction(WalkAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopWalk);
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &APlayerCharacter::StartAim);
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &APlayerCharacter::StopAim);
		if (ReloadAction)
		{
			Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &APlayerCharacter::Reload);
		}
		if (InteractAction)
		{
			Input->BindAction(InteractAction, ETriggerEvent::Started, this, &APlayerCharacter::Interact);
		}
	}
}

void APlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller) { return; }

	// Use only the controller's yaw so looking down doesn't push the character into the floor
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y); // forward / backward
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X); // left / right
}

void APlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const UMuseumUserSettings* Settings = UMuseumUserSettings::Get();
	const float Sensitivity = Settings ? Settings->MouseSensitivity : 1.f;
	AddControllerYawInput(Axis.X * Sensitivity);
	AddControllerPitchInput(Axis.Y * Sensitivity * ((Settings && Settings->bInvertY) ? -1.f : 1.f));
}

void APlayerCharacter::StartWalk()
{
	bWantsToWalk = true;
	UpdateMovementSpeed();
}

void APlayerCharacter::StopWalk()
{
	bWantsToWalk = false;
	UpdateMovementSpeed();
}

void APlayerCharacter::StartAim()
{
	bIsAiming = true;
	UpdateMovementSpeed();
}

void APlayerCharacter::StopAim()
{
	bIsAiming = false;
	UpdateMovementSpeed();
}

void APlayerCharacter::UpdateMovementSpeed()
{
	// Aiming also slows the character down to a walk for a steadier shot
	GetCharacterMovement()->MaxWalkSpeed = ((bWantsToWalk || bIsAiming) ? WalkSpeed : JogSpeed) * MoveSpeedMult;
}

void APlayerCharacter::Fire()
{
	// While carrying an object the fire button throws it instead of shooting
	if (IsHoldingObject())
	{
		ReleaseObject(true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !Controller || !ProjectileClass || bIsDead || bIsReloading) { return; }

	// Empty magazine: reload instead of shooting
	if (AmmoInMagazine <= 0)
	{
		Reload();
		return;
	}

	// Fire rate limit
	const float Now = World->GetTimeSeconds();
	if (Now - LastFireTime < FireInterval / FireRateMult) { return; }
	LastFireTime = Now;

	--AmmoInMagazine;
	OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);

	if (FireMontage)
	{
		PlayAnimMontage(FireMontage);
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySound2D(this, FireSound);
	}
	// Bolt animation on every shot disabled for Lab 6 (the bolt moves only when reloading); uncomment to bring it back
	// if (WeaponFireAnim)
	// {
	// 	WeaponMesh->PlayAnimation(WeaponFireAnim, false);
	// }

	// The point under the crosshair: trace from the camera through the centre of the screen
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * MaxAimDistance;

	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AimTrace), false, this);
	FHitResult AimHit;
	const bool bHit = World->LineTraceSingleByChannel(AimHit, ViewLocation, TraceEnd, ECC_Projectile, TraceParams);
	const FVector TargetPoint = bHit ? AimHit.ImpactPoint : TraceEnd;

	// The bullet starts at the muzzle and flies to that point, so it lands where the crosshair is
	const FVector MuzzleLocation = WeaponMesh->DoesSocketExist(MuzzleSocketName)
		? WeaponMesh->GetSocketLocation(MuzzleSocketName)
		: ViewLocation + ViewRotation.Vector() * 50.f;

	// Muzzle flash: sparks along the shot and a short flash of light
	if (MuzzleFlashFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MuzzleFlashFX, MuzzleLocation, (TargetPoint - MuzzleLocation).Rotation());
	}
	MuzzleLight->SetVisibility(true);
	GetWorldTimerManager().SetTimer(MuzzleLightTimer,
		FTimerDelegate::CreateWeakLambda(this, [this]() { MuzzleLight->SetVisibility(false); }), MuzzleLightTime, false);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AMuseumProjectile* Projectile = World->SpawnActor<AMuseumProjectile>(
		ProjectileClass, MuzzleLocation, (TargetPoint - MuzzleLocation).Rotation(), SpawnParams))
	{
		Projectile->SetDamageScale(DamageMultiplier);
		Projectile->Launch(TargetPoint);
	}

	// Recoil: the muzzle climbs a little and the view kicks; aiming steadies it
	RecoilVelocity += bIsAiming ? 12.f : 24.f;
	ShotFovKick += 1.4f;

	// Guards notice a shot aimed at them and may try to dodge it
	for (TActorIterator<AGuardNPC> It(World); It; ++It)
	{
		It->OnShotFired(this, MuzzleLocation, TargetPoint);
	}
}

FGenericTeamId APlayerCharacter::GetGenericTeamId() const
{
	return TeamID.GetId();
}

void APlayerCharacter::SetInHidingZone(bool bInside)
{
	HidingZoneCount = FMath::Max(HidingZoneCount + (bInside ? 1 : -1), 0);
}

float APlayerCharacter::GetAimPitch() const
{
	// The controller stores pitch as 0..360; the Aim Offset expects -90..90
	return FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);
}

void APlayerCharacter::Reload()
{
	if (bIsDead || bIsReloading || AmmoInMagazine >= MagazineSize || ReserveAmmo <= 0) { return; }

	bIsReloading = true;
	OnReloadStateChanged(true);

	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!ReloadMontage || !Anim || Anim->Montage_Play(ReloadMontage, EffectiveReloadRate()) <= 0.f)
	{
		// No montage to play: fill the magazine at once
		LoadRounds(MagazineSize);
		FinishReload();
		return;
	}
	FOnMontageBlendingOutStarted BlendingOut = FOnMontageBlendingOutStarted::CreateUObject(this, &APlayerCharacter::OnReloadMontageBlendingOut);
	Anim->Montage_SetBlendingOutDelegate(BlendingOut, ReloadMontage);

	// The shadow body plays the same montage; the pistol pulls its bolt back and keeps it open for the clip
	if (UAnimInstance* BodyAnim = BodyMesh->GetAnimInstance())
	{
		BodyAnim->Montage_Play(ReloadMontage, EffectiveReloadRate());
	}
	if (WeaponUnlockAnim)
	{
		WeaponMesh->PlayAnimation(WeaponUnlockAnim, false);
		WeaponMesh->SetPosition(BoltOpenStartTime, false);
		WeaponMesh->SetPlayRate(EffectiveReloadRate());
		bBoltOpen = true;
		if (BoltBackSound)
		{
			UGameplayStatics::PlaySound2D(this, BoltBackSound);
		}
	}

	bFreshClipTaken = false;
}

void APlayerCharacter::UpdateReloadClip(float DeltaTime)
{
	// Mauser C96: the left hand pulls the empty clip out of the guide, swaps it for a full one in the pocket and sets that
	// into the guide on top of the open action (ReloadStart); the thumb pushes the rounds down into the magazine (ReloadLoop,
	// the hand follows the top round); the hand lets go and the bolt closes (ReloadEnd), the empty clip stays in the guide.
	const UPlayerAnimInstance* Anim = Cast<UPlayerAnimInstance>(GetMesh()->GetAnimInstance());
	const bool bActive = bIsReloading && Anim && ReloadMontage && Anim->Montage_IsPlaying(ReloadMontage);
	const FName Section = bActive ? Anim->Montage_GetCurrentSection(ReloadMontage) : NAME_None;
	const float Alpha = Anim ? Anim->GetLeftHandIKAlpha() : 0.f;
	// The clip is seated in the guide: the montage moves on from ReloadStart to the loop
	if (ClipInSound && PrevReloadSection == TEXT("ReloadStart") && Section == TEXT("ReloadLoop"))
	{
		UGameplayStatics::PlaySound2D(this, ClipInSound);
	}
	PrevReloadSection = Section;
	// Out of sight in the pocket the empty clip becomes a full one (10 rounds, fewer when the reserve is low)
	if (bActive && !bFreshClipTaken && Section == TEXT("ReloadStart") && Anim->Montage_GetPosition(ReloadMontage) >= ClipTakeTime)
	{
		bFreshClipTaken = true;
		if (ClipOutSound)
		{
			UGameplayStatics::PlaySound2D(this, ClipOutSound);
		}
		RoundsInClip = FMath::Min(ClipCapacity, ReserveAmmo);
		RoundsPushed = 0;
		ClipStackShift = 0.f;
		DrawnStackShift = -1.f;
	}

	// The bolt goes back as in a shot and is held there while loading
	if (bBoltOpen && WeaponMesh->IsPlaying() && WeaponMesh->GetPosition() >= BoltCloseTime)
	{
		WeaponMesh->Stop();
	}
	// The hand has let go (or the reload stopped): the bolt goes forward and chambers a round
	if (bBoltOpen && (!bActive || (Section == TEXT("ReloadEnd") && Alpha <= 0.f)) && WeaponReloadAnim)
	{
		bBoltOpen = false;
		if (BoltForwardSound && !bIsDead)
		{
			UGameplayStatics::PlaySound2D(this, BoltForwardSound);
		}
		WeaponMesh->PlayAnimation(WeaponReloadAnim, false);
		WeaponMesh->SetPosition(BoltCloseTime, false);
		WeaponMesh->SetPlayRate(EffectiveReloadRate());
	}

	// The clip lives inside the pistol between reloads: it rises out of the guide at the start and is pushed back in at the end
	const float SinkDepth = ClipHeight * ClipScale + 0.5f;
	const float Time = bActive ? Anim->Montage_GetPosition(ReloadMontage) : 0.f;
	ClipSink = !bActive ? SinkDepth
		: Section == TEXT("ReloadStart") ? SinkDepth * (1.f - FMath::SmoothStep(ClipRiseTime.X, ClipRiseTime.Y, Time))
		: Section == TEXT("ReloadEnd") ? SinkDepth * FMath::SmoothStep(ClipSinkTime.X, ClipSinkTime.Y, Time)
		: 0.f;
	ClipMesh->SetVisibility(ClipSink < SinkDepth, true);

	// Pushed rounds slide down out of the clip; the ones fully inside the magazine are not drawn (rebuilt only while they move)
	ClipStackShift = FMath::FInterpConstantTo(ClipStackShift, RoundsPushed * ClipRoundPitch, DeltaTime * EffectiveReloadRate(), RoundSlideSpeed);
	ClipStackDrop = ((ClipCapacity - RoundsInClip) * ClipRoundPitch + ClipStackShift) * ClipScale;
	if (ClipStackShift != DrawnStackShift)
	{
		DrawnStackShift = ClipStackShift;
		ClipRounds->ClearInstances();
		for (int32 i = 0; i < RoundsInClip; ++i)
		{
			const float Z = ClipFirstRoundZ + i * ClipRoundPitch - ClipStackShift;
			if (Z > -ClipRoundRadius)
			{
				ClipRounds->AddInstance(FTransform(FVector(0.f, 0.f, Z)));
			}
		}
	}

	// Attached, not placed by hand, so the clip moves in the same frame as the hand or the pistol: in the clip guide while the
	// hand is fully there (the hand reaches that pose exactly, so the switch does not show), otherwise held by the left hand
	const bool bInGuide = !Anim || Anim->IsClipInGuide();
	USceneComponent* Parent = bInGuide ? WeaponMesh.Get() : GetMesh();
	const FName Socket = bInGuide ? ClipGuideSocketName : FName(TEXT("hand_l"));
	if (ClipMesh->GetAttachParent() != Parent || ClipMesh->GetAttachSocketName() != Socket)
	{
		ClipMesh->AttachToComponent(Parent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	}
	FTransform Clip = bInGuide ? FTransform(FVector(0.f, 0.f, -ClipSink)) : Anim->GetClipInHand(ClipStackDrop);
	Clip.SetScale3D(FVector(ClipScale));
	ClipMesh->SetRelativeTransform(Clip);
}

void APlayerCharacter::OnReloadNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (!bIsReloading || bIsDead || NotifyName != TEXT("RoundsInserted")) { return; }

	if (ClipPushSounds.Num() > 0)
	{
		UGameplayStatics::PlaySound2D(this, ClipPushSounds[FMath::RandHelper(ClipPushSounds.Num())]);
	}

	const int32 AmmoBefore = AmmoInMagazine;
	LoadRounds(RoundsPerCycle);
	RoundsPushed += AmmoInMagazine - AmmoBefore;

	// Magazine full or nothing left: leave the loop, the next section raises the pistol
	if (AmmoInMagazine >= MagazineSize || ReserveAmmo <= 0)
	{
		for (UAnimInstance* Anim : { GetMesh()->GetAnimInstance(), BodyMesh->GetAnimInstance() })
		{
			if (Anim)
			{
				Anim->Montage_SetNextSection(TEXT("ReloadLoop"), TEXT("ReloadEnd"), ReloadMontage);
			}
		}
	}
}

void APlayerCharacter::LoadRounds(int32 Count)
{
	const int32 Loaded = FMath::Min3(Count, MagazineSize - AmmoInMagazine, ReserveAmmo);
	if (Loaded <= 0) { return; }

	AmmoInMagazine += Loaded;
	ReserveAmmo -= Loaded;
	OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);
}

void APlayerCharacter::OnReloadMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	// Also when interrupted: the rounds already pushed in stay in the magazine
	FinishReload();
}

void APlayerCharacter::FinishReload()
{
	if (!bIsReloading) { return; }

	bIsReloading = false;
	OnReloadStateChanged(false);
}

float APlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead || bGodMode) { return 0.f; }

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.f) { return 0.f; }

	Health = FMath::Clamp(Health - Applied, 0.f, MaxHealth);
	OnHealthChanged(Health, MaxHealth);

	if (UMuseumDirector* Director = UMuseumDirector::Get(this))
	{
		Director->NotifyPlayerHurt(Applied);
	}
	if (DamageCauser)
	{
		DamageIndicators.Add({ DamageCauser->GetActorLocation(), 0.f });
		if (DamageIndicators.Num() > 6) { DamageIndicators.RemoveAt(0); }
	}

	// The hit must be obvious: red flash on the screen and a camera shake
	if (HUDWidget)
	{
		HUDWidget->PlayDamageFlash();
	}
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		const UMuseumUserSettings* Settings = UMuseumUserSettings::Get();
		if (HitCameraShake && (!Settings || Settings->bCameraShake))
		{
			PC->ClientStartCameraShake(HitCameraShake);
		}
	}

	if (Health <= 0.f)
	{
		Die();
	}
	return Applied;
}

void APlayerCharacter::Die()
{
	ReleaseObject(false);
	bIsDead = true;
	bIsReloading = false;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// The game director shows the game over screen; without it the level simply restarts
	if (UMuseumDirector* Director = UMuseumDirector::Get(this))
	{
		Director->NotifyPlayerDied();
	}
	else
	{
		GetWorldTimerManager().SetTimer(RestartTimer, this, &APlayerCharacter::RestartLevel, RestartDelay, false);
	}
}

void APlayerCharacter::RestartLevel()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

bool APlayerCharacter::AddHealth(float Amount)
{
	if (bIsDead || Amount <= 0.f || Health >= MaxHealth) { return false; }

	Health = FMath::Min(Health + Amount, MaxHealth);
	OnHealthChanged(Health, MaxHealth);
	return true;
}

int32 APlayerCharacter::AddAmmo(int32 Amount)
{
	const int32 FreeSpace = MaxReserveAmmo - ReserveAmmo;
	if (bIsDead || Amount <= 0 || FreeSpace <= 0) { return 0; }

	const int32 Added = FMath::Min(Amount, FreeSpace);
	ReserveAmmo += Added;
	OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);

	// Picked up bullets with an empty pistol: load them straight away
	if (AmmoInMagazine <= 0)
	{
		Reload();
	}
	return Added;
}

void APlayerCharacter::ShowHUDMessage(const FText& Message)
{
	if (HUDWidget && !Message.IsEmpty())
	{
		HUDWidget->ShowMessage(Message);
	}
}

bool APlayerCharacter::IsHoldingObject() const
{
	return PhysicsHandle && PhysicsHandle->GetGrabbedComponent() != nullptr;
}

void APlayerCharacter::Interact()
{
	if (bIsDead) { return; }

	if (IsHoldingObject())
	{
		ReleaseObject(false);
	}
	else
	{
		GrabObject();
	}
}

UPrimitiveComponent* APlayerCharacter::TraceForPhysicsObject() const
{
	if (!Controller) { return nullptr; }

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrabTrace), false, this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * GrabDistance,
		ECC_Visibility, Params))
	{
		return nullptr;
	}

	UPrimitiveComponent* Component = Hit.GetComponent();
	return (Component && Component->IsSimulatingPhysics()) ? Component : nullptr;
}

void APlayerCharacter::GrabObject()
{
	UPrimitiveComponent* Target = TraceForPhysicsObject();
	// Heavy objects (chests, doors) can only be pushed
	if (!Target || Target->GetMass() > MaxGrabMass) { return; }

	// The carried object must not push the player or lift them when it ends up under the feet
	HeldObjectPawnResponse = Target->GetCollisionResponseToChannel(ECC_Pawn);
	Target->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Target->WakeAllRigidBodies();

	// Hold it by the middle and keep its current turn relative to the view
	HeldObjectYawOffset = FRotator::NormalizeAxis(Target->GetComponentRotation().Yaw - Controller->GetControlRotation().Yaw);
	PhysicsHandle->GrabComponentAtLocationWithRotation(Target, NAME_None, Target->Bounds.Origin, Target->GetComponentRotation());
}

void APlayerCharacter::UpdateHeldObject()
{
	if (!IsHoldingObject() || !Controller) { return; }

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TargetLocation = ViewLocation + ViewRotation.Vector() * HoldDistance;

	// The object got stuck behind a wall or a door: let it go
	UPrimitiveComponent* Held = PhysicsHandle->GetGrabbedComponent();
	if (FVector::Dist(Held->Bounds.Origin, TargetLocation) > GrabDistance)
	{
		ReleaseObject(false);
		return;
	}

	// Carried upright, turning together with the player
	PhysicsHandle->SetTargetLocationAndRotation(TargetLocation, FRotator(0.f, ViewRotation.Yaw + HeldObjectYawOffset, 0.f));
}

void APlayerCharacter::ReleaseObject(bool bThrow)
{
	UPrimitiveComponent* Held = PhysicsHandle ? PhysicsHandle->GetGrabbedComponent() : nullptr;
	if (!Held) { return; }

	PhysicsHandle->ReleaseComponent();
	Held->SetCollisionResponseToChannel(ECC_Pawn, HeldObjectPawnResponse);

	if (bThrow && Controller)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		// Velocity change: light and heavy objects fly at the same speed
		Held->AddImpulse(ViewRotation.Vector() * ThrowSpeed, NAME_None, true);
	}
}

void APlayerCharacter::UpdateInteractionHint()
{
	FText Hint;
	if (IsHoldingObject())
	{
		Hint = HoldingHint;
	}
	else if (const UPrimitiveComponent* Target = TraceForPhysicsObject())
	{
		Hint = Target->GetMass() <= MaxGrabMass ? PickUpHint : HeavyObjectHint;
	}

	// The HUD is updated only when the hint changes
	if (!Hint.EqualTo(CurrentHint))
	{
		CurrentHint = Hint;
		if (HUDWidget)
		{
			HUDWidget->SetInteractionHint(Hint);
		}
	}
}

void APlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// A hard landing dips the camera
	LandDip = FMath::Clamp(-LastFallSpeed / 900.f, 0.f, 1.f) * 7.f;
}

void APlayerCharacter::UpdateCameraFeel(float DeltaTime)
{
	const UMuseumUserSettings* Settings = UMuseumUserSettings::Get();
	const bool bBob = !Settings || Settings->bHeadBob;

	// Head bob: a figure-eight that follows the steps, only on the ground
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();
	const float MoveAlpha = (!bIsDead && Movement->IsMovingOnGround()) ? FMath::Clamp(Speed / JogSpeed, 0.f, 1.f) : 0.f;
	BobAlpha = FMath::FInterpTo(BobAlpha, MoveAlpha, DeltaTime, 8.f);
	BobPhase += DeltaTime * ((bWantsToWalk || bIsAiming) ? 7.5f : 10.f) * MoveAlpha;
	const float BobScale = bBob ? (bIsAiming ? 0.35f : 1.f) : 0.f;
	const float BobZ = FMath::Sin(BobPhase * 2.f) * 1.5f * BobAlpha * BobScale;
	const float BobY = FMath::Sin(BobPhase) * 1.1f * BobAlpha * BobScale;

	if (Movement->IsFalling()) { LastFallSpeed = GetVelocity().Z; }
	LandDip = FMath::FInterpTo(LandDip, 0.f, DeltaTime, 7.f);

	// Death: the camera drops to the floor and tilts
	DeathAlpha = FMath::FInterpTo(DeathAlpha, bIsDead ? 1.f : 0.f, DeltaTime, 2.5f);
	FirstPersonCamera->SetRelativeLocation(CameraBaseLocation + FVector(0.f, BobY, BobZ - LandDip - DeathAlpha * 52.f));

	// Recoil pushes the view up, then it settles back
	if (Controller && (FMath::Abs(RecoilVelocity) > 0.01f || RecoilOffset > 0.01f))
	{
		const float Kick = RecoilVelocity * DeltaTime;
		RecoilVelocity = FMath::FInterpTo(RecoilVelocity, 0.f, DeltaTime, 16.f);
		const float Recover = RecoilOffset * FMath::Min(1.f, 6.f * DeltaTime);
		RecoilOffset = FMath::Max(RecoilOffset + Kick - Recover, 0.f);

		FRotator Rotation = Controller->GetControlRotation();
		Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) + Kick - Recover, -80.f, 80.f);
		Controller->SetControlRotation(Rotation);
	}
	ShotFovKick = FMath::FInterpTo(ShotFovKick, 0.f, DeltaTime, 10.f);

	// A little lean into the strafe, and the fall on death
	if (Controller)
	{
		const float Lean = bBob && !bIsDead ? FVector::DotProduct(GetVelocity(), GetActorRightVector()) / JogSpeed * 1.2f : 0.f;
		StrafeRoll = FMath::FInterpTo(StrafeRoll, Lean + DeathAlpha * 28.f, DeltaTime, 6.f);
		FRotator Rotation = Controller->GetControlRotation();
		Rotation.Roll = StrafeRoll;
		Controller->SetControlRotation(Rotation);
	}

	// Hit marker, damage numbers and direction arcs age here
	HitMarkerAge += DeltaTime;
	for (FDamageNumber& Number : DamageNumbers) { Number.Age += DeltaTime; }
	DamageNumbers.RemoveAll([](const FDamageNumber& Number) { return Number.Age > 0.9f; });
	for (FDamageIndicator& Indicator : DamageIndicators) { Indicator.Age += DeltaTime; }
	DamageIndicators.RemoveAll([](const FDamageIndicator& Indicator) { return Indicator.Age > 1.2f; });
}

void APlayerCharacter::RegisterHit(const FVector& Where, float Damage, bool bHeadshot, bool bKilled)
{
	HitMarkerAge = 0.f;
	bHitMarkerHeadshot = bHeadshot;
	bHitMarkerKill = bKilled;

	const UMuseumUserSettings* Settings = UMuseumUserSettings::Get();
	if (!Settings || Settings->bDamageNumbers)
	{
		DamageNumbers.Add({ Where, Damage, 0.f, bHeadshot, bKilled });
		if (DamageNumbers.Num() > 14) { DamageNumbers.RemoveAt(0); }
	}
	if (UMuseumDirector* Director = UMuseumDirector::Get(this))
	{
		Director->PlayHitFeedback(bHeadshot, bKilled);
	}
}

void APlayerCharacter::ApplyPerk(EMuseumPerk Perk)
{
	switch (Perk)
	{
	case EMuseumPerk::Damage: DamageMultiplier *= 1.2f; break;
	case EMuseumPerk::FireRate: FireRateMult *= 1.15f; break;
	case EMuseumPerk::Reload: ReloadRateMult *= 1.25f; break;
	case EMuseumPerk::Ammo:
		MaxReserveAmmo += 30;
		AddAmmo(30);
		break;
	case EMuseumPerk::Health:
		MaxHealth += 25.f;
		Health = FMath::Min(Health + 40.f, MaxHealth);
		OnHealthChanged(Health, MaxHealth);
		break;
	case EMuseumPerk::Speed:
		MoveSpeedMult *= 1.08f;
		UpdateMovementSpeed();
		break;
	default:
		break;
	}
}

void APlayerCharacter::ToggleFlashlight()
{
	if (Flashlight && !bIsDead)
	{
		Flashlight->SetVisibility(!Flashlight->IsVisible());
	}
}

void APlayerCharacter::SetHUDVisible(bool bVisible)
{
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void APlayerCharacter::ResetForRun()
{
	ReleaseObject(false);
	GetWorldTimerManager().ClearTimer(RestartTimer);

	DamageMultiplier = FireRateMult = ReloadRateMult = MoveSpeedMult = 1.f;
	if (BaseMaxHealth > 0.f) { MaxHealth = BaseMaxHealth; }
	if (BaseMaxReserveAmmo > 0) { MaxReserveAmmo = BaseMaxReserveAmmo; }

	// Alive again: input, movement, animations
	bIsDead = false;
	bIsReloading = false;
	bBoltOpen = false;
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) { Anim->Montage_Stop(0.f); }
	if (UAnimInstance* BodyAnim = BodyMesh->GetAnimInstance()) { BodyAnim->Montage_Stop(0.f); }
	if (APlayerController* PC = Cast<APlayerController>(Controller)) { EnableInput(PC); }
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	DeathAlpha = 0.f;
	RecoilVelocity = RecoilOffset = ShotFovKick = 0.f;
	HitMarkerAge = 100.f;
	DamageNumbers.Reset();
	DamageIndicators.Reset();

	Health = MaxHealth;
	AmmoInMagazine = MagazineSize;
	ReserveAmmo = StartReserveAmmo;
	UpdateMovementSpeed();
	OnHealthChanged(Health, MaxHealth);
	OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);
	OnReloadStateChanged(false);
}

void APlayerCharacter::OnHealthChanged_Implementation(float NewHealth, float InMaxHealth)
{
	if (HUDWidget)
	{
		HUDWidget->SetHealth(NewHealth, InMaxHealth);
	}
}

void APlayerCharacter::OnAmmoChanged_Implementation(int32 InAmmoInMagazine, int32 InMagazineSize, int32 InReserveAmmo)
{
	if (HUDWidget)
	{
		HUDWidget->SetAmmo(InAmmoInMagazine, InMagazineSize, InReserveAmmo);
	}
}

void APlayerCharacter::OnReloadStateChanged_Implementation(bool bReloading)
{
	// The HUD shows "Reloading..." while the reload montage plays
	if (HUDWidget)
	{
		HUDWidget->SetReloading(bReloading);
	}
}
