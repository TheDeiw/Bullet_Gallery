// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "InputActionValue.h"
#include "MuseumTeams.h"
#include "MuseumTypes.h"
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
class UPhysicsHandleComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS()
class BULLET_GALLERY_API APlayerCharacter : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	APlayerCharacter();

	/** The player belongs to PlayersTeam, so the guards' perception treats them as an enemy */
	virtual FGenericTeamId GetGenericTeamId() const override;

	/** A floating damage number over an enemy that was hit (drawn by the HUD) */
	struct FDamageNumber
	{
		FVector Location;
		float Amount;
		float Age;
		bool bHeadshot;
		bool bKill;
	};

	/** Where the last hits on the player came from (drawn by the HUD as red arcs) */
	struct FDamageIndicator
	{
		FVector Source;
		float Age;
	};

	/** Flashlight on the camera (key F): the museum is dark at night */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void ToggleFlashlight();

	/** Shows or hides the health / ammo HUD (hidden in the main menu and on the end screens) */
	void SetHUDVisible(bool bVisible);

	/** Starts a new run: alive, full health and ammo, no perks */
	void ResetForRun();

	/** One level of a perk chosen between the waves */
	void ApplyPerk(EMuseumPerk Perk);

	void SetGodMode(bool bOn) { bGodMode = bOn; }

	/** An enemy was hit by this player's bullet: hit marker, damage number, sound */
	void RegisterHit(const FVector& Where, float Damage, bool bHeadshot, bool bKilled);

	const TArray<FDamageNumber>& GetDamageNumbers() const { return DamageNumbers; }
	const TArray<FDamageIndicator>& GetDamageIndicators() const { return DamageIndicators; }
	float GetHitMarkerAge() const { return HitMarkerAge; }
	bool IsHitMarkerHeadshot() const { return bHitMarkerHeadshot; }
	bool IsHitMarkerKill() const { return bHitMarkerKill; }
	float GetMaxHealth() const { return MaxHealth; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }

	/** Called by a blind zone when the player walks in (true) or out (false) */
	void SetInHidingZone(bool bInside);

	/** True while the player stands in a blind zone: the guards cannot see them there */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsHiddenFromAI() const { return HidingZoneCount > 0; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Fire();
	void StartWalk();
	void StopWalk();
	void StartAim();
	void StopAim();
	void Reload();
	/** Montage Notify "RoundsInserted" of ReloadMontage: one pass of the loop section pushed rounds in */
	UFUNCTION()
	void OnReloadNotify(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
	/** Moves up to Count rounds from the reserve into the magazine */
	void LoadRounds(int32 Count);
	/** The reload montage started to blend out (finished or interrupted): firing is allowed again */
	void OnReloadMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void FinishReload();
	/** Moves the stripper clip from the left hand into the clip guide and slides the pushed rounds into the magazine */
	void UpdateReloadClip(float DeltaTime);
	void Die();
	void RestartLevel();

	/** Picks jog or walk speed depending on the walk key and aiming */
	void UpdateMovementSpeed();

	/** Head bob, landing dip, recoil, death camera, hit marker timers (every frame) */
	void UpdateCameraFeel(float DeltaTime);

	/** Reload speed with the perks */
	float EffectiveReloadRate() const { return ReloadPlayRate * ReloadRateMult; }

	// ---------- Perks (reset by ResetForRun) ----------
	float DamageMultiplier = 1.f;
	float FireRateMult = 1.f;
	float ReloadRateMult = 1.f;
	float MoveSpeedMult = 1.f;
	float BaseMaxHealth = 0.f;
	int32 BaseMaxReserveAmmo = 0;
	bool bGodMode = false;

	// ---------- Camera feel ----------
	FVector CameraBaseLocation = FVector(15.f, 0.f, 70.f);
	float BobPhase = 0.f;
	float BobAlpha = 0.f;
	float LandDip = 0.f;
	float LastFallSpeed = 0.f;
	float RecoilVelocity = 0.f;
	float RecoilOffset = 0.f;
	float ShotFovKick = 0.f;
	float DeathAlpha = 0.f;
	float StrafeRoll = 0.f;

	// ---------- Hit feedback ----------
	float HitMarkerAge = 100.f;
	bool bHitMarkerHeadshot = false;
	bool bHitMarkerKill = false;
	TArray<FDamageNumber> DamageNumbers;
	TArray<FDamageIndicator> DamageIndicators;

	/** Creates the HUD widget and adds it to the viewport (once a player controller owns the character) */
	void CreateHUDWidget();

	/** E: picks up the physics object under the crosshair, or drops the one being carried */
	void Interact();
	void GrabObject();
	/** Lets go of the carried object; with bThrow it is thrown where the camera looks */
	void ReleaseObject(bool bThrow);
	/** Moves the carried object in front of the camera (every frame) */
	void UpdateHeldObject();
	/** Physics body under the crosshair within GrabDistance, if any */
	UPrimitiveComponent* TraceForPhysicsObject() const;
	/** Shows on the HUD what the E key would do right now */
	void UpdateInteractionHint();

	/** Collision response to pawns of the carried object before it was picked up */
	TEnumAsByte<ECollisionResponse> HeldObjectPawnResponse = ECR_Block;
	/** Yaw of the carried object relative to the view, kept while carrying */
	float HeldObjectYawOffset = 0.f;
	FText CurrentHint;

	bool bWantsToWalk = false;
	bool bIsAiming = false;
	bool bIsReloading = false;
	bool bIsDead = false;
	float LastFireTime = -1000.f;
	float Health = 100.f;
	int32 AmmoInMagazine = 0;
	int32 ReserveAmmo = 0;
	FTimerHandle RestartTimer;
	FTimerHandle MuzzleLightTimer;

	/** Reload state of the stripper clip: rounds it had at the start, rounds pushed into the magazine so far */
	int32 RoundsInClip = 0;
	int32 RoundsPushed = 0;
	/** How far the stack of rounds has slid down, and how far its top is below the top of a full clip, cm */
	float ClipStackShift = 0.f;
	float ClipStackDrop = 0.f;
	/** How deep the clip is pushed into the pistol (below the clip guide), cm; fully in = hidden */
	float ClipSink = 0.f;
	/** Stack shift the round instances were last built for (they are rebuilt only when it changes) */
	float DrawnStackShift = -1.f;
	bool bBoltOpen = false;
	/** The empty clip has been swapped for a full one in the pocket during this reload */
	bool bFreshClipTaken = false;

	UPROPERTY(Transient)
	TObjectPtr<UMuseumHUDWidget> HUDWidget;

	/** Team of the player (PlayersTeam) */
	FPlayerTeamID TeamID;

	/** Number of blind zones the player stands in (they can overlap) */
	int32 HidingZoneCount = 0;

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	/** Medkit: restores health up to MaxHealth. Returns false if the health is already full */
	UFUNCTION(BlueprintCallable, Category = "Health")
	bool AddHealth(float Amount);

	/** Ammo box: adds bullets to the reserve up to MaxReserveAmmo. Returns how many were added */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 AddAmmo(int32 Amount);

	/** Short message in the middle of the screen (pickups, teleports) */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowHUDMessage(const FText& Message);

	/** True while the player carries a physics object */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsHoldingObject() const;

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

	/** Spot light on the camera; follows the view */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight")
	TObjectPtr<USpotLightComponent> Flashlight;

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

	/** The reserve cannot hold more than this; extra bullets from ammo boxes are left behind */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 0))
	int32 MaxReserveAmmo = 90;

	/**
	 * Reload montage (AM_Player_Reload): ReloadStart lowers the pistol, ReloadLoop repeats while rounds are pushed in
	 * (each pass fires the "RoundsInserted" notify), ReloadEnd raises it. Without it the magazine is filled at once.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** Rounds pushed in during one pass of the ReloadLoop section (half a stripper clip) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 1))
	int32 RoundsPerCycle = 5;

	/** Animation of the pistol at the start of the reload: played from BoltOpenStartTime, the bolt goes back and is held at BoltCloseTime */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponUnlockAnim;   // A_Fire (the bolt part of a shot)

	/** Animation of the pistol at the end of the reload: played from BoltCloseTime, the bolt goes forward */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponReloadAnim;   // A_Fire

	/** Time in WeaponUnlockAnim where the bolt starts to move back, seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 0))
	float BoltOpenStartTime = 0.4f;

	/** Time where the bolt is fully back (held there while loading) and starts to close, seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 0))
	float BoltCloseTime = 0.5f;

	/** Stripper clip (SM_StripperClip): in the left hand, then in the clip guide while the rounds are pushed in */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> ClipMesh;

	/** Rounds in the clip (instances of SM_ClipRound); they slide down into the magazine as the thumb pushes them */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UInstancedStaticMeshComponent> ClipRounds;

	/** How far the top round is below the top of a full clip, cm (the left hand follows it) */
	float GetClipStackDrop() const { return ClipStackDrop + ClipSink; }

	/** Speed of the reload animation, 1 = normal */
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = 0.05))
	float ReloadPlayRate = 0.7f;

	/** Debug: empties the magazine and starts a reload, so the reload can be checked in PIE without the keyboard */
	UPROPERTY(EditAnywhere, Category = "Weapon|Debug")
	bool bDebugReload = false;

	/** Debug: fires one shot (muzzle flash check in PIE without the mouse) */
	UPROPERTY(EditAnywhere, Category = "Weapon|Debug")
	bool bDebugFire = false;

	/** Socket on the weapon mesh at the top of the action (the clip guide) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FName ClipGuideSocketName = TEXT("ClipGuide");

	/** Montage time when the left hand has taken the clip from the belt, seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = 0))
	float ClipTakeTime = 0.48f;

	/** Sparks at the muzzle on every shot (NS_MuzzleFlash), spawned along the shot direction */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UNiagaraSystem> MuzzleFlashFX;

	/** Short flash of light at the muzzle on every shot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	/** How long the muzzle light stays on, seconds */
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = 0.01))
	float MuzzleLightTime = 0.05f;

	/** Animation of the pistol itself: the slide on a shot */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponFireAnim;   // A_Fire

	// ---------- Sounds (all played in 2D: they come from the player's own body and weapon) ----------

	/** Mauser shot */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> FireSound;

	/** Bolt pulled back at the start of the reload */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> BoltBackSound;

	/** The empty clip is taken out and swapped for a full one (ClipTakeTime of the reload montage) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> ClipOutSound;

	/** The full clip is set into the clip guide (ReloadStart -> ReloadLoop) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> ClipInSound;

	/** Rounds pushed from the clip into the magazine, once per "RoundsInserted" notify; a random one of these each time */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> ClipPushSounds;

	/** Bolt pushed forward at the end of the reload */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> BoltForwardSound;

	/** Footsteps: a random one of these per step */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	/** Distance walked between two steps while jogging, cm */
	UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = 10))
	float JogStepDistance = 170.f;

	/** Distance walked between two steps while walking or aiming, cm */
	UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = 10))
	float WalkStepDistance = 110.f;

	/** Calm background music: looped, non-spatial, started when the player appears (the sound must have Looping on) */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> MusicSound;

	UPROPERTY(EditDefaultsOnly, Category = "Audio", meta = (ClampMin = 0))
	float MusicVolume = 0.4f;

	/** Distance travelled since the last step, cm */
	float StepTravelled = 0.f;

	/** Reload montage section of the previous tick (to notice ReloadStart -> ReloadLoop) */
	FName PrevReloadSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = 1))
	float MaxHealth = 100.f;

	/** Health at the start of the level (can be lower than MaxHealth so medkits are useful) */
	UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = 1))
	float StartHealth = 100.f;

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

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;   // IA_Interact (E)

	/** Holds a picked-up physics object in front of the camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

	/** How far from the camera an object can be picked up, cm */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float GrabDistance = 300.f;

	/** Distance from the camera to a carried object, cm */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float HoldDistance = 150.f;

	/** Objects heavier than this cannot be lifted, only pushed, kg */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = 0))
	float MaxGrabMass = 40.f;

	/** Speed given to a thrown object, cm/s */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float ThrowSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	FText PickUpHint = NSLOCTEXT("Player", "PickUpHint", "[E] Pick up");

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	FText HeavyObjectHint = NSLOCTEXT("Player", "HeavyHint", "Too heavy to lift - push it");

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	FText HoldingHint = NSLOCTEXT("Player", "HoldingHint", "[E] Drop    [LMB] Throw");

};
