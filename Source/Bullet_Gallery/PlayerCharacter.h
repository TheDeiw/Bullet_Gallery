// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class UInputMappingContext;
class UInputAction;
class AMuseumProjectile;

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

	/** Picks jog or walk speed depending on the walk key and aiming */
	void UpdateMovementSpeed();

	bool bWantsToWalk = false;
	bool bIsAiming = false;
	float LastFireTime = -1000.f;

public:
	/** Camera pitch for the Aim Offset in the Animation Blueprint (-90..90) */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetAimPitch() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAiming() const { return bIsAiming; }

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

};
