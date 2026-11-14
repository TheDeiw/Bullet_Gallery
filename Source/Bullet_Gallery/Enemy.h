#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MuseumTypes.h"
#include "Enemy.generated.h"

class UAnimSequenceBase;
class UEnemyHealthBarComponent;
class UNiagaraSystem;
class UPointLightComponent;
class USoundAttenuation;
class USoundBase;

/** What the enemy is doing right now */
UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Patrol,  // walks along its route
	Chase,   // walks towards the player
	Attack,  // plays a blow
	Dead
};

/**
 * Base enemy of the museum: a statue that came to life.
 * No artificial intelligence is used (no AI controller, behaviour tree or navigation mesh):
 * the enemy has no controller at all and moves itself with AddMovementInput, following
 * a fixed state machine in Tick:
 * Patrol (route points) -> Chase (player seen nearby) -> Attack (player in reach) -> Patrol (player lost).
 */
UCLASS()
class BULLET_GALLERY_API AEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemy();

	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	/** Route in world space, set by the spawn manager. bLoop = false walks it back and forth */
	void SetPatrolRoute(const TArray<FVector>& WorldPoints, bool bLoop);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	EEnemyState GetEnemyState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsDead() const { return State == EEnemyState::Dead; }

	/** Tougher / faster / bigger version of the Blueprint's enemy (call before BeginPlay, i.e. between deferred spawn and finish) */
	void ApplyVariant(const FEnemyVariant& InVariant);

	/** 0..1 */
	float GetHealthFraction() const;

protected:
	virtual void BeginPlay() override;

	void UpdatePatrol(float DeltaTime);
	void UpdateChase(float DeltaTime);
	void UpdateAttack(float DeltaTime);
	void SetState(EEnemyState NewState);

	/** The player, or nullptr if there is none or the player is dead */
	APawn* GetLivePlayer() const;

	/** Nothing solid between the statue's eyes and the player */
	bool CanSeePlayer() const;

	void MoveTowards(const FVector& Location);
	/** Follows a navigation-mesh path to the goal (rebuilt twice a second), straight when the goal is near and in sight */
	void MoveAlongNav(const FVector& Goal, float DeltaTime);
	void FaceTowards(const FVector& Location, float DeltaTime);
	int32 FindNearestRoutePoint() const;

	void StartAttack();
	/** The moment the blow lands: damages the player if still in reach and in front */
	void ApplyAttackHit();
	void FinishAttack();

	void Die();
	/** Called when the death animation is over: the statue crumbles into dust */
	void Shatter();

	/** Plays an animation in the slot of the Animation Blueprint; returns its duration in seconds */
	float PlayAnimInSlot(UAnimSequenceBase* Anim, float PlayRate = 1.f);

	static UAnimSequenceBase* PickRandom(const TArray<TObjectPtr<UAnimSequenceBase>>& Anims);

	/** Health bar above the head; it also counts the damage taken */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UEnemyHealthBarComponent> HealthBar;

	// ---------- Patrol ----------

	UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
	float PatrolSpeed = 160.f;

	/** Pause at every route point, seconds */
	UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
	float WaitAtPointTime = 1.5f;

	/** A route point counts as reached at this distance, cm */
	UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
	float AcceptanceRadius = 50.f;

	// ---------- Chase ----------

	UPROPERTY(EditAnywhere, Category = "Enemy|Chase")
	float ChaseSpeed = 330.f;

	/** The player is noticed inside this radius if nothing blocks the view, cm */
	UPROPERTY(EditAnywhere, Category = "Enemy|Chase")
	float DetectionRadius = 1200.f;

	/** Played once when a patrolling statue notices the player and starts the chase */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Chase")
	TObjectPtr<USoundBase> AlertSound;

	/** Punch landing on the player (ApplyAttackHit): a random one of these */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Attack")
	TArray<TObjectPtr<USoundBase>> PunchSounds;

	/** Falloff of AlertSound */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Chase")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	/** The chase stops when the player is farther than this, cm */
	UPROPERTY(EditAnywhere, Category = "Enemy|Chase")
	float LoseRadius = 1800.f;

	/** The chase stops when the player stays out of sight this long, seconds */
	UPROPERTY(EditAnywhere, Category = "Enemy|Chase")
	float LoseSightTime = 3.f;

	/** After being shot, the enemy chases the player this long even from far away, seconds */
	UPROPERTY(EditAnywhere, Category = "Enemy|Chase")
	float AggroTime = 6.f;

	// ---------- Attack ----------

	/** Distance between the centres of the enemy and the player for a blow, cm */
	UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
	float AttackRange = 150.f;

	/** Small damage per blow (the player has 100 health) */
	UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
	float AttackDamage = 8.f;

	/** Pause between two blows, seconds */
	UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
	float AttackCooldown = 1.2f;

	/** When the blow lands, as a part of the attack animation (0 = start, 1 = end) */
	UPROPERTY(EditAnywhere, Category = "Enemy|Attack", meta = (ClampMin = 0, ClampMax = 1))
	float AttackHitFraction = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
	float AttackPlayRate = 1.f;

	// ---------- Animation ----------

	/** Slot node of the Animation Blueprint used for blows and hit reactions */
	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	FName AnimSlotName = TEXT("DefaultSlot");

	/** One of them is picked at random for every blow */
	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	TArray<TObjectPtr<UAnimSequenceBase>> AttackAnims;

	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	TArray<TObjectPtr<UAnimSequenceBase>> HitReactAnims;

	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	TArray<TObjectPtr<UAnimSequenceBase>> DeathAnims;

	/** The enemy stands still this long after a bullet hits it, seconds */
	UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
	float HitStunTime = 0.35f;

	// ---------- Effects ----------

	/** Shown in front of the player when a blow lands */
	UPROPERTY(EditAnywhere, Category = "Enemy|Effects")
	TObjectPtr<UNiagaraSystem> AttackHitEffect;

	/** Bursts over the body when the statue crumbles after death */
	UPROPERTY(EditAnywhere, Category = "Enemy|Effects")
	TObjectPtr<UNiagaraSystem> DeathEffect;

	UPROPERTY(EditAnywhere, Category = "Enemy|Effects")
	FVector DeathEffectScale = FVector(4.f);

	/** Number of bursts spread over the body, so the whole statue crumbles and not one point */
	UPROPERTY(EditAnywhere, Category = "Enemy|Effects", meta = (ClampMin = 1))
	int32 DeathEffectCount = 5;

	/** Pause between the end of the death animation and the crumbling, seconds */
	UPROPERTY(EditAnywhere, Category = "Enemy|Effects")
	float ShatterDelay = 0.4f;

private:
	FEnemyVariant Variant;
	bool bHunter = false;
	bool bLastHitHeadshot = false;
	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	float RepathTimer = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> AuraLight;

	EEnemyState State = EEnemyState::Patrol;

	TArray<FVector> PatrolRoute;
	int32 PatrolIndex = 0;
	int32 PatrolStep = 1;
	bool bLoopRoute = true;

	float WaitTimeLeft = 0.f;
	float StunTimeLeft = 0.f;
	float AggroTimeLeft = 0.f;
	float TimeSinceSeen = 0.f;
	float LastAttackTime = -1000.f;

	TWeakObjectPtr<APawn> Player;

	FTimerHandle AttackHitTimer;
	FTimerHandle AttackEndTimer;
	FTimerHandle ShatterTimer;
};
