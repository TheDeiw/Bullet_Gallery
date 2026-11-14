#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "MuseumTeams.h"
#include "MuseumTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "GuardNPC.generated.h"

class AGuardAIController;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAnimMontage;
class UBlackboardComponent;
class UEnemyHealthBarComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USkeletalMeshComponent;

/**
 * Museum night guard: an NPC enemy driven by artificial intelligence (the "AAI_Character" of the lab guide).
 * Unlike the living statues of Lab 3 (a hand-written state machine without a controller), the guard is
 * possessed by an AI controller that runs a behaviour tree:
 * - AIPerception (sight) notices the player, who belongs to the hostile PlayersTeam;
 * - the character writes what it perceives into the Blackboard (HasLineOfSight, DetectedEnemy, LastKnownLocation);
 * - the behaviour tree decides: patrol and look around, chase and shoot, check the last known location, or dodge.
 */
UCLASS()
class BULLET_GALLERY_API AGuardNPC : public ACharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AGuardNPC();

	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// ---------- Affiliation ----------

	virtual FGenericTeamId GetGenericTeamId() const override { return TeamID.GetId(); }

	/** Any actor of PlayersTeam is hostile, everything else is neutral (and so ignored by the sight sense) */
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	// ---------- Movement (used by the BTT_SetSpeed Blueprint task) ----------

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void Walk();

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void Run();

	UFUNCTION(BlueprintPure, Category = "Movement")
	bool IsRunning() const { return bIsRunning; }

	/** A poison zone slows the guard down: 1 = normal speed */
	void SetSpeedMultiplier(float Multiplier);

	// ---------- Combat ----------

	/** Turns to the target and fires the revolver once. Returns true if the bullet hit the target */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool FireAt(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetWeaponEffectiveRange() const { return WeaponEffectiveRange; }

	/** 0 = revolver lowered, 1 = aimed; read by UGuardAnimInstance every frame */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetWeaponRaiseAlpha() const { return WeaponRaiseAlpha; }

	/** The guard wants the revolver up: the player is seen within range */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWeaponRaised() const { return bWeaponRaised; }

	/** The revolver is (almost) fully raised: only then can the guard shoot */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWeaponReady() const { return WeaponRaiseAlpha >= 0.95f; }

	/** The player fired from From towards To. If the shot passes close, the guard gets ready to dodge */
	void OnShotFired(AActor* Shooter, const FVector& From, const FVector& To);

	/** Jumps aside: Side = +1 to the right, -1 to the left. The guard cannot be hit while dodging */
	void Dodge(float Side);

	/** True if there is free space for a dodge on that side (+1 right, -1 left) */
	bool CanDodgeTo(float Side) const;

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodging() const { return bIsDodging; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bIsDead; }

	/** Tougher / weaker version of the Blueprint's guard (call between deferred spawn and finish) */
	void ApplyVariant(const FEnemyVariant& InVariant);

	/** Wave guards start with the player already noticed and keep knowing where they are */
	void AlertTo(AActor* Target);

	float GetDodgeDuration() const { return DodgeDuration; }

protected:
	virtual void BeginPlay() override;

	/** Bound to AIPerceptionComponent::OnTargetPerceptionUpdated: the guard saw or lost someone */
	UFUNCTION()
	void OnActorPerceived(AActor* Actor, FAIStimulus Stimulus);

	/** The sight sense sees the actor and the actor does not hide in a blind zone */
	bool CanSee(AActor* Actor) const;

	/** Recomputes the line of sight and writes it to the Blackboard when it changes */
	void UpdateLineOfSight();

	void Die();
	void FinishDodge();

	/** Applies the walk / run speed multiplied by the poison slowdown */
	void ApplyMovementSpeed();

	/** Decides whether the revolver should be up and moves WeaponRaiseAlpha towards it */
	void UpdateWeaponRaise(float DeltaTime);

	UBlackboardComponent* GetBlackboard() const;

	// ---------- Components ----------

	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> PerceptionComponent;

	/** Sight sense of the perception component (radius, view angle, detection by affiliation) */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Revolver attached to the hand socket as a separate asset */
	UPROPERTY(Category = "Weapon", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	/** Health bar above the head: its own widget (UGuardHealthBarWidget), unlike the statues' bars */
	UPROPERTY(Category = "Health", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyHealthBarComponent> HealthBar;

	// ---------- Perception state ----------

	/** The player the guard noticed; cleared when the guard forgets them */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AI")
	TObjectPtr<AActor> PerceivedActor;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AI")
	bool bHasLineOfSight = false;

	/** The guard forgets the player after not seeing them this long, seconds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI")
	float TargetLostTime = 8.f;

	/** How often the last known location is refreshed while the player is visible, seconds */
	UPROPERTY(EditAnywhere, Category = "AI")
	float LastLocationUpdateTime = 0.5f;

	/** A shot passing closer than this to the guard is noticed (and may be dodged), cm */
	UPROPERTY(EditAnywhere, Category = "AI")
	float ShotNoticeRadius = 150.f;

	// ---------- Sounds (3D, heard from where the guard stands) ----------

	/** Revolver shot, played at the muzzle in FireAt */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> ShotSound;

	/** Played once when the guard goes from "nobody noticed" to a target */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> AlertSound;

	/** Falloff of the two sounds above */
	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	/** PerceivedActor was set on the last Tick (the alert sound has been played for it) */
	bool bWasAlerted = false;

	// ---------- Movement ----------

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Movement")
	float RunSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Movement")
	float WalkSpeed = 0.f;

	/** Walk speed as a part of the run speed. 0.75 of the 400 cm/s run is the 300 cm/s walk speed of BS_Idle_Walk_Run */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = 0.1, ClampMax = 1))
	float WalkSpeedFraction = 0.75f;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Movement")
	bool bIsRunning = false;

	// ---------- Weapon ----------

	/** The guard comes this close to the player and shoots, cm */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon")
	float WeaponEffectiveRange = 900.f;

	/** Seconds to raise the revolver from the lowered pose into the aim (and to lower it back) */
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = 0.05))
	float RaiseTime = 0.4f;

	/** The revolver goes up when the seen player is closer than WeaponEffectiveRange * RaiseRangeFactor
	 *  (the same tolerance as the Shoot Target task, so the guard never waits in range with a lowered gun) */
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = 0.1))
	float RaiseRangeFactor = 1.25f;

	/** ...and goes down only beyond WeaponEffectiveRange * LowerRangeFactor or when the player is not seen (no flicker at the edge) */
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = 0.1))
	float LowerRangeFactor = 1.5f;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Weapon")
	bool bWeaponRaised = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Weapon")
	float WeaponRaiseAlpha = 0.f;

	/** Small damage of one shot (the player has 100 health) */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float WeaponDamage = 5.f;

	/** Random spread of the shots, degrees */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	float WeaponSpread = 2.5f;

	/** Socket on the skeleton (hand_r) that holds the revolver */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FName WeaponSocketName = TEXT("WeaponPoint");

	/** Muzzle of the revolver in the weapon mesh's local space (the model has no muzzle socket) */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FVector MuzzleOffset = FVector(0.f, 18.f, 3.f);

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UNiagaraSystem> MuzzleEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	// ---------- Dodge ----------

	/** Sideways speed of the dodge jump, cm/s */
	UPROPERTY(EditAnywhere, Category = "Dodge")
	float DodgeSpeed = 950.f;

	/** The dodge lasts this long; the guard cannot be hit meanwhile, seconds */
	UPROPERTY(EditAnywhere, Category = "Dodge")
	float DodgeDuration = 0.45f;

	/** Free space needed on the side of the dodge, cm */
	UPROPERTY(EditAnywhere, Category = "Dodge")
	float DodgeClearance = 220.f;

	// ---------- Death ----------

	/** The body stays on the floor this long after death, seconds */
	UPROPERTY(EditAnywhere, Category = "Health")
	float CorpseLifeSpan = 10.f;

private:
	/** Team of the guards (GuardsTeam) */
	FGuardTeamID TeamID;

	FEnemyVariant Variant;
	bool bHunter = false;
	bool bLastHitHeadshot = false;
	float HunterRefreshElapsed = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<AGuardAIController> GuardController;

	/** The last actor reported by the sight sense, seen or lost */
	TWeakObjectPtr<AActor> SightTarget;

	float SpeedMultiplier = 1.f;
	float TargetLostElapsed = 0.f;
	float LastLocationElapsed = 0.f;
	bool bIsDodging = false;
	bool bIsDead = false;

	FTimerHandle DodgeTimer;
};
