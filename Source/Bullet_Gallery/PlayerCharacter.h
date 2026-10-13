// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class UAnimSequenceBase;
class UInputMappingContext;
class UInputAction;
class UCameraShakeBase;
class AMuseumProjectile;
class UMuseumHUDWidget;

UCLASS()
class BULLET_GALLERY_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APlayerCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Fire();
	void StartWalk();
	void StopWalk();
	void StartAim();
	void StopAim();
	void Reload();
	void FinishReload();
	void Die();
	void RestartLevel();

	/** Picks jog or walk speed depending on the walk key and aiming */
	void UpdateMovementSpeed();

	/** Creates the HUD widget and adds it to the viewport (once a player controller owns the character) */
	void CreateHUDWidget();

	bool bWantsToWalk = false;
	bool bIsAiming = false;
	bool bIsReloading = false;
	bool bIsDead = false;
	float LastFireTime = -1000.f;
	float Health = 100.f;
	int32 AmmoInMagazine = 0;
	int32 ReserveAmmo = 0;
	FTimerHandle ReloadTimer;
	FTimerHandle RestartTimer;

	UPROPERTY(Transient)
	TObjectPtr<UMuseumHUDWidget> HUDWidget;

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	/** Health changed (hit by an enemy). The C++ part updates the HUD; a Blueprint can add more */
	UFUNCTION(BlueprintNativeEvent, Category = "Health")
	void OnHealthChanged(float NewHealth, float InMaxHealth);

	/** Ammo changed: a shot or a reload. The C++ part updates the HUD; a Blueprint can add more */
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnAmmoChanged(int32 InAmmoInMagazine, int32 InMagazineSize, int32 InReserveAmmo);

	/** Reload started (true) or finished (false) */
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnReloadStateChanged(bool bReloading);

	/** Camera pitch for the Aim Offset in the Animation Blueprint (-90..90) */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetAimPitch() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAiming() const { return bIsAiming; }

	/**
	 * Second copy of the body, with the head. The owner never sees it (so it cannot block the camera),
	 * but it casts the shadow, so the shadow has a head. The first mesh (Mesh) has the head bone hidden
	 * and is seen only by the owner.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	/** First-person camera at eye level */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Weapon model (Mauser C96) attached to the hand socket */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Socket on the character skeleton (hand_r) that holds the weapon */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FName WeaponSocketName = TEXT("WeaponPoint");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UAnimMontage> FireMontage;      // MM_Pistol_Fire_Montage

	/** Bullet spawned on every shot */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AMuseumProjectile> ProjectileClass;

	/** Socket on the weapon mesh where the bullet appears */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Minimum time between shots, seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float FireInterval = 0.4f;

	/** How far the crosshair trace looks for a target, cm */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float MaxAimDistance = 30000.f;

	/** The Mauser C96 is loaded with a 10-round stripper clip */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 1))
	int32 MagazineSize = 10;

	/** Spare bullets at the start, not counting the loaded magazine */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 0))
	int32 StartReserveAmmo = 60;

	/** Reload time, seconds (there is no reload animation) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 0.1))
	float ReloadTime = 1.f;

	/** Animation of the pistol itself: the slide on a shot */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponFireAnim;   // A_Fire

	UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = 1))
	float MaxHealth = 100.f;

	/** Camera shake played when an enemy hits the player */
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	TSubclassOf<UCameraShakeBase> HitCameraShake;

	/** After death the level restarts in this many seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float RestartDelay = 3.f;

	/** HUD with the health and ammo bars (WBP_HUD) */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UMuseumHUDWidget> HUDWidgetClass;

	/** Default movement speed (jog) */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float JogSpeed = 400.f;

	/** Speed while the walk key is held or while aiming */
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float WalkSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float DefaultFOV = 90.f;

	/** Narrower field of view while aiming (zoom) */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float AimFOV = 60.f;

	/** How fast the camera blends between the default and aiming FOV */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float AimInterpSpeed = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;   // IMC_Default

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MouseLookMappingContext; // IMC_MouseLook

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;       // IA_Move

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;       // IA_Look (gamepad)

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MouseLookAction;  // IA_MouseLook (mouse)

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;       // IA_Jump

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;       // IA_Fire

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> WalkAction;       // IA_Walk (hold)

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;        // IA_Aim (hold)

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;     // IA_Reload

};
