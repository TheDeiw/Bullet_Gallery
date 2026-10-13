#include "PlayerCharacter.h"

#include "Bullet_Gallery.h"
#include "MuseumHUDWidget.h"
#include "MuseumProjectile.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

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

	// Camera at eye level, slightly in front of the head
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(15.f, 0.f, 70.f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	// Weapon: created in the constructor so it is visible in the Blueprint viewport
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), WeaponSocketName);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// Called when the game starts or when spawned
void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetMesh()->HideBoneByName(TEXT("head"), EPhysBodyOp::PBO_None);

	// Attach the weapon to the skeleton socket (also picks up a socket name overridden in the Blueprint)
	WeaponMesh->AttachToComponent(GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocketName);

	UpdateMovementSpeed();

	Health = MaxHealth;
	AmmoInMagazine = MagazineSize;
	ReserveAmmo = StartReserveAmmo;

	CreateHUDWidget();
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
	const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;
	FirstPersonCamera->SetFieldOfView(
		FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaTime, AimInterpSpeed));
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
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
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
	GetCharacterMovement()->MaxWalkSpeed = (bWantsToWalk || bIsAiming) ? WalkSpeed : JogSpeed;
}

void APlayerCharacter::Fire()
{
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
	if (Now - LastFireTime < FireInterval) { return; }
	LastFireTime = Now;

	--AmmoInMagazine;
	OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);

	if (FireMontage)
	{
		PlayAnimMontage(FireMontage);
	}
	if (WeaponFireAnim)
	{
		WeaponMesh->PlayAnimation(WeaponFireAnim, false);
	}

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

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AMuseumProjectile* Projectile = World->SpawnActor<AMuseumProjectile>(
		ProjectileClass, MuzzleLocation, (TargetPoint - MuzzleLocation).Rotation(), SpawnParams))
	{
		Projectile->Launch(TargetPoint);
	}
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

	float Duration = ReloadTime;
	if (ReloadAnim)
	{
		// Played in the arms slot, so the legs keep walking
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->PlaySlotAnimationAsDynamicMontage(ReloadAnim, ReloadSlotName, 0.15f, 0.2f);
		}
		if (Duration <= 0.f)
		{
			Duration = ReloadAnim->GetPlayLength();
		}
	}
	if (WeaponReloadAnim)
	{
		WeaponMesh->PlayAnimation(WeaponReloadAnim, false);
	}

	GetWorldTimerManager().SetTimer(ReloadTimer, this, &APlayerCharacter::FinishReload, FMath::Max(Duration, 0.1f), false);
}

void APlayerCharacter::FinishReload()
{
	if (bIsDead) { return; }

	// Move bullets from the reserve into the magazine
	const int32 Loaded = FMath::Min(MagazineSize - AmmoInMagazine, ReserveAmmo);
	AmmoInMagazine += Loaded;
	ReserveAmmo -= Loaded;

	bIsReloading = false;
	OnReloadStateChanged(false);
	OnAmmoChanged(AmmoInMagazine, MagazineSize, ReserveAmmo);
}

float APlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead) { return 0.f; }

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.f) { return 0.f; }

	Health = FMath::Clamp(Health - Applied, 0.f, MaxHealth);
	OnHealthChanged(Health, MaxHealth);

	// The hit must be obvious: red flash on the screen and a camera shake
	if (HUDWidget)
	{
		HUDWidget->PlayDamageFlash();
	}
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (HitCameraShake)
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
	bIsDead = true;
	bIsReloading = false;
	GetWorldTimerManager().ClearTimer(ReloadTimer);

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	GetWorldTimerManager().SetTimer(RestartTimer, this, &APlayerCharacter::RestartLevel, RestartDelay, false);
}

void APlayerCharacter::RestartLevel()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
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
	if (HUDWidget)
	{
		HUDWidget->SetReloading(bReloading);
	}
}
