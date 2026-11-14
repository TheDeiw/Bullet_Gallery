#include "Enemy.h"

#include "EnemyHealthBarComponent.h"
#include "MuseumDirector.h"
#include "PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MuseumAudio.h"
#include "Sound/SoundBase.h"
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

	if (!SoundAttenuation) { SoundAttenuation = NewNpcSoundAttenuation(this); }

	Player = UGameplayStatics::GetPlayerPawn(this, 0);
	SetState(EEnemyState::Patrol);

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (bHunter)
	{
		// Wave enemies know where the player is from the first second and never lose track of them
		AggroTimeLeft = 1.e9f;
		SetState(EEnemyState::Chase);
		if (DeathEffect)
		{
			// The statue crumbles out of the dark: the same dust as at death, smaller
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DeathEffect, GetActorLocation() - FVector(0.f, 0.f, HalfHeight * 0.9f),
				FVector::UpVector.Rotation(), DeathEffectScale * 0.5f * Variant.Scale);
		}
	}

	// Glowing eyes / body of the special variants: easy to tell apart in the dark halls
	if (Variant.Aura.A > 0.f)
	{
		AuraLight = NewObject<UPointLightComponent>(this, TEXT("AuraLight"));
		AuraLight->SetupAttachment(GetCapsuleComponent());
		AuraLight->SetRelativeLocation(FVector(35.f, 0.f, HalfHeight * 0.45f / Variant.Scale));
		AuraLight->IntensityUnits = ELightUnits::Candelas;
		AuraLight->SetIntensity(18.f * Variant.Scale);
		AuraLight->SetLightColor(Variant.Aura.ToFColor(true));
		AuraLight->SetAttenuationRadius(330.f * Variant.Scale);
		AuraLight->SetCastShadows(false);
		AuraLight->RegisterComponent();
	}
}

void AEnemy::ApplyVariant(const FEnemyVariant& InVariant)
{
	Variant = InVariant;
	bHunter = InVariant.bHunter;

	PatrolSpeed *= InVariant.SpeedMul;
	ChaseSpeed *= InVariant.SpeedMul;
	AttackDamage *= InVariant.DamageMul;
	// A bigger statue reaches further and notices things from further away
	AttackRange *= InVariant.Scale;
	AcceptanceRadius *= InVariant.Scale;
	HealthBar->SetMaxHealth(HealthBar->GetMaxHealth() * InVariant.HealthMul);
	GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
	UE_LOG(LogTemp, Display, TEXT("Enemy variant %s: health %.0f, chase speed %.0f, damage %.1f"), *InVariant.Name.ToString(), HealthBar->GetMaxHealth(), ChaseSpeed, AttackDamage);
}

float AEnemy::GetHealthFraction() const
{
	return HealthBar ? HealthBar->GetHealthPercent() : 0.f;
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
	// Patrol -> Chase is the moment the statue notices the player
	if (State == EEnemyState::Patrol && NewState == EEnemyState::Chase && AlertSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, AlertSound, GetActorLocation(), 1.f, 1.f, 0.f, SoundAttenuation);
	}

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

	MoveAlongNav(PlayerLocation, DeltaTime);
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

void AEnemy::MoveAlongNav(const FVector& Goal, float DeltaTime)
{
	const FVector Me = GetActorLocation();

	// Close and in plain sight: walk straight, the path would only add detours
	if (FVector::Dist(Goal, Me) < 700.f && CanSeePlayer())
	{
		PathPoints.Reset();
		MoveTowards(Goal);
		return;
	}

	RepathTimer -= DeltaTime;
	if (RepathTimer <= 0.f)
	{
		RepathTimer = FMath::FRandRange(0.45f, 0.7f);
		PathPoints.Reset();
		PathIndex = 0;
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			const UNavigationPath* Path = Nav->FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), Goal, this);
			if (Path && Path->IsValid())
			{
				// The first point is where the enemy stands
				for (int32 i = 1; i < Path->PathPoints.Num(); ++i) { PathPoints.Add(Path->PathPoints[i]); }
			}
		}
	}

	while (PathPoints.IsValidIndex(PathIndex) && FVector::Dist2D(PathPoints[PathIndex], Me) < 90.f * Variant.Scale)
	{
		++PathIndex;
	}
	// No navigation mesh here (or the path ran out): straight at the goal
	MoveTowards(PathPoints.IsValidIndex(PathIndex) ? PathPoints[PathIndex] : Goal);
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

	if (PunchSounds.Num() > 0)
	{
		UGameplayStatics::PlaySoundAtLocation(this, PunchSounds[FMath::RandHelper(PunchSounds.Num())], Target->GetActorLocation());
	}

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

	// A bullet that lands in the upper part of the body is a headshot: double damage
	FVector HitLocation = GetActorLocation();
	bool bHeadshot = false;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		HitLocation = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.ImpactPoint;
		bHeadshot = HitLocation.Z > GetActorLocation().Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.42f;
		if (bHeadshot) { DamageAmount *= 2.f; }
	}

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.f) { return 0.f; }

	// The health bar component counts the damage and redraws itself
	const bool bKilled = HealthBar->ApplyDamage(Applied);
	bLastHitHeadshot = bHeadshot;
	if (APlayerCharacter* Shooter = Cast<APlayerCharacter>(EventInstigator ? EventInstigator->GetPawn() : nullptr))
	{
		Shooter->RegisterHit(HitLocation, Applied, bHeadshot, bKilled);
	}
	if (bKilled)
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

	// Flinch, unless in the middle of a blow (the heavy ones do not flinch at all)
	const bool bHeavy = Variant.Kind == EEnemyKind::Brute || Variant.Kind == EEnemyKind::Boss;
	if (!bHeavy && State != EEnemyState::Attack && PlayAnimInSlot(PickRandom(HitReactAnims)) > 0.f)
	{
		StunTimeLeft = HitStunTime;
	}
	return Applied;
}

void AEnemy::Die()
{
	SetState(EEnemyState::Dead);
	if (UMuseumDirector* Director = UMuseumDirector::Get(this))
	{
		Director->NotifyEnemyKilled(this, Variant, bLastHitHeadshot);
	}
	if (AuraLight) { AuraLight->SetVisibility(false); }
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
