#include "Enemy.h"

#include "EnemyHealthBarComponent.h"
#include "PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

AEnemy::AEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// No controller at all (and so no AI): the enemy drives its own movement component
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bRunPhysicsWithNoController = true;
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
	Movement->MaxWalkSpeed = PatrolSpeed;
	bUseControllerRotationYaw = false;

	// The mesh stands with its feet at the bottom of the capsule, facing forward
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight), FRotator(0.f, -90.f, 0.f));

	// Health bar a little above the head
	HealthBar = CreateDefaultSubobject<UEnemyHealthBarComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetCapsuleComponent());
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight + 35.f));
}

void AEnemy::BeginPlay()
{
	Super::BeginPlay();

	Player = UGameplayStatics::GetPlayerPawn(this, 0);
	SetState(EEnemyState::Patrol);
}

void AEnemy::SetPatrolRoute(const TArray<FVector>& WorldPoints, bool bLoop)
{
	PatrolRoute = WorldPoints;
	bLoopRoute = bLoop;
	PatrolIndex = 0;
	PatrolStep = 1;
}

void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State == EEnemyState::Dead) { return; }

	// The player pawn can appear later than the enemy
	if (!Player.IsValid())
	{
		Player = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	// Stands still for a moment after a bullet hit
	if (StunTimeLeft > 0.f)
	{
		StunTimeLeft -= DeltaTime;
		return;
	}

	switch (State)
	{
	case EEnemyState::Patrol: UpdatePatrol(DeltaTime); break;
	case EEnemyState::Chase:  UpdateChase(DeltaTime);  break;
	case EEnemyState::Attack: UpdateAttack(DeltaTime); break;
	default: break;
	}
}

void AEnemy::SetState(EEnemyState NewState)
{
	State = NewState;
	GetCharacterMovement()->MaxWalkSpeed = (NewState == EEnemyState::Chase) ? ChaseSpeed : PatrolSpeed;

	if (NewState == EEnemyState::Chase)
	{
		TimeSinceSeen = 0.f;
	}
}

void AEnemy::UpdatePatrol(float DeltaTime)
{
	// Notice the player nearby
	if (const APawn* Target = GetLivePlayer())
	{
		if (FVector::Dist(Target->GetActorLocation(), GetActorLocation()) <= DetectionRadius && CanSeePlayer())
		{
			SetState(EEnemyState::Chase);
			return;
		}
	}

	// No route: the statue guards its spawn point
	if (PatrolRoute.Num() == 0) { return; }

	if (WaitTimeLeft > 0.f)
	{
		WaitTimeLeft -= DeltaTime;
		return;
	}

	const FVector Target = PatrolRoute[PatrolIndex];
	if (FVector::Dist2D(Target, GetActorLocation()) > AcceptanceRadius)
	{
		MoveTowards(Target);
		return;
	}

	// Point reached: wait, then go to the next one (in a loop or back and forth)
	WaitTimeLeft = WaitAtPointTime;
	if (PatrolRoute.Num() > 1)
	{
		if (bLoopRoute)
		{
			PatrolIndex = (PatrolIndex + 1) % PatrolRoute.Num();
		}
		else
		{
			if (!PatrolRoute.IsValidIndex(PatrolIndex + PatrolStep))
			{
				PatrolStep = -PatrolStep;
			}
			PatrolIndex += PatrolStep;
		}
	}
}

void AEnemy::UpdateChase(float DeltaTime)
{
	const APawn* Target = GetLivePlayer();
	if (!Target)
	{
		SetState(EEnemyState::Patrol);
		return;
	}

	const FVector PlayerLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist(PlayerLocation, GetActorLocation());
	TimeSinceSeen = CanSeePlayer() ? 0.f : TimeSinceSeen + DeltaTime;

	// Give up: the player ran away or hid. Return to the closest point of the route
	AggroTimeLeft = FMath::Max(AggroTimeLeft - DeltaTime, 0.f);
	if (AggroTimeLeft <= 0.f && (Distance > LoseRadius || TimeSinceSeen > LoseSightTime))
	{
		PatrolIndex = FindNearestRoutePoint();
		SetState(EEnemyState::Patrol);
		return;
	}

	if (Distance <= AttackRange)
	{
		FaceTowards(PlayerLocation, DeltaTime);
		if (GetWorld()->GetTimeSeconds() - LastAttackTime >= AttackCooldown)
		{
			StartAttack();
		}
		return;
	}

	MoveTowards(PlayerLocation);
}

void AEnemy::UpdateAttack(float DeltaTime)
{
	// Keep turning to the player while swinging
	if (const APawn* Target = GetLivePlayer())
	{
		FaceTowards(Target->GetActorLocation(), DeltaTime);
	}
}

APawn* AEnemy::GetLivePlayer() const
{
	APawn* Pawn = Player.Get();
	if (const APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(Pawn))
	{
		if (PlayerCharacter->IsDead()) { return nullptr; }
	}
	return Pawn;
}

bool AEnemy::CanSeePlayer() const
{
	const APawn* Target = Player.Get();
	if (!Target) { return false; }

	const FVector Eyes = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySight), false, this);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Target->GetActorLocation(), ECC_Visibility, Params);
}

void AEnemy::MoveTowards(const FVector& Location)
{
	// Straight line on the floor plane; the movement component handles floors, steps and walls
	const FVector Direction = (Location - GetActorLocation()).GetSafeNormal2D();
	AddMovementInput(Direction, 1.f);
}

void AEnemy::FaceTowards(const FVector& Location, float DeltaTime)
{
	const FVector Direction = (Location - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero()) { return; }

	const FRotator TargetRotation(0.f, Direction.Rotation().Yaw, 0.f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, 8.f));
}

int32 AEnemy::FindNearestRoutePoint() const
{
	int32 Nearest = 0;
	float BestDistance = TNumericLimits<float>::Max();
	for (int32 i = 0; i < PatrolRoute.Num(); ++i)
	{
		const float Distance = FVector::DistSquared2D(PatrolRoute[i], GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Nearest = i;
		}
	}
	return Nearest;
}

void AEnemy::StartAttack()
{
	SetState(EEnemyState::Attack);
	LastAttackTime = GetWorld()->GetTimeSeconds();

	float Duration = PlayAnimInSlot(PickRandom(AttackAnims), AttackPlayRate);
	if (Duration <= 0.f)
	{
		Duration = 1.f;
	}

	// The damage is dealt in the middle of the swing, not when it starts
	GetWorldTimerManager().SetTimer(AttackHitTimer, this, &AEnemy::ApplyAttackHit,
		FMath::Max(Duration * AttackHitFraction, 0.01f), false);
	GetWorldTimerManager().SetTimer(AttackEndTimer, this, &AEnemy::FinishAttack, Duration, false);
}

void AEnemy::ApplyAttackHit()
{
	APawn* Target = GetLivePlayer();
	if (!Target || IsDead()) { return; }

	// The player may have stepped back or aside during the swing
	const FVector ToPlayer = Target->GetActorLocation() - GetActorLocation();
	if (ToPlayer.Size() > AttackRange + 50.f) { return; }
	if (FVector::DotProduct(GetActorForwardVector(), ToPlayer.GetSafeNormal2D()) < 0.3f) { return; }

	UGameplayStatics::ApplyDamage(Target, AttackDamage, nullptr, this, UDamageType::StaticClass());

	if (AttackHitEffect)
	{
		// Just in front of the player's face, so the hit is seen in first person
		const FVector HitPoint = Target->GetActorLocation() - ToPlayer.GetSafeNormal2D() * 35.f + FVector(0.f, 0.f, 50.f);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, AttackHitEffect, HitPoint, (-ToPlayer).Rotation());
	}
}

void AEnemy::FinishAttack()
{
	if (State == EEnemyState::Attack)
	{
		SetState(EEnemyState::Chase);
	}
}

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead()) { return 0.f; }

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.f) { return 0.f; }

	// The health bar component counts the damage and redraws itself
	if (HealthBar->ApplyDamage(Applied))
	{
		Die();
		return Applied;
	}

	// A bullet always gives the player away, even from behind or from far away
	AggroTimeLeft = AggroTime;
	if (State == EEnemyState::Patrol)
	{
		SetState(EEnemyState::Chase);
	}

	// Flinch, unless in the middle of a blow
	if (State != EEnemyState::Attack && PlayAnimInSlot(PickRandom(HitReactAnims)) > 0.f)
	{
		StunTimeLeft = HitStunTime;
	}
	return Applied;
}

void AEnemy::Die()
{
	SetState(EEnemyState::Dead);
	GetWorldTimerManager().ClearTimer(AttackHitTimer);
	GetWorldTimerManager().ClearTimer(AttackEndTimer);

	HealthBar->SetVisibility(false);

	// Bullets and the player pass through the falling statue
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	float Duration = 0.f;
	if (UAnimSequenceBase* DeathAnim = PickRandom(DeathAnims))
	{
		// Single-node mode keeps the last frame instead of blending back to idle
		GetMesh()->PlayAnimation(DeathAnim, false);
		Duration = DeathAnim->GetPlayLength();
	}

	GetWorldTimerManager().SetTimer(ShatterTimer, this, &AEnemy::Shatter, Duration + ShatterDelay, false);
}

void AEnemy::Shatter()
{
	if (DeathEffect)
	{
		// Bursts at random points of the body: the whole statue turns to dust
		const FBox Bounds = GetMesh()->Bounds.GetBox();
		for (int32 i = 0; i < DeathEffectCount; ++i)
		{
			const FVector Location = (i == 0) ? Bounds.GetCenter() : FMath::RandPointInBox(Bounds);
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DeathEffect, Location,
				FVector::UpVector.Rotation(), DeathEffectScale);
		}
	}

	Destroy();
}

float AEnemy::PlayAnimInSlot(UAnimSequenceBase* Anim, float PlayRate)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!Anim || !AnimInstance) { return 0.f; }

	PlayRate = FMath::Max(PlayRate, 0.01f);
	AnimInstance->PlaySlotAnimationAsDynamicMontage(Anim, AnimSlotName, 0.15f, 0.2f, PlayRate);
	return Anim->GetPlayLength() / PlayRate;
}

UAnimSequenceBase* AEnemy::PickRandom(const TArray<TObjectPtr<UAnimSequenceBase>>& Anims)
{
	return Anims.Num() > 0 ? Anims[FMath::RandRange(0, Anims.Num() - 1)].Get() : nullptr;
}
