#include "GuardNPC.h"

#include "Bullet_Gallery.h"
#include "EnemyHealthBarComponent.h"
#include "GuardAIController.h"
#include "GuardHealthBarWidget.h"
#include "PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

namespace
{
	/** The player hides in a blind zone or is already dead: the guards ignore them */
	bool IsHiddenFromGuards(const AActor* Actor)
	{
		const APlayerCharacter* Player = Cast<APlayerCharacter>(Actor);
		return Player && (Player->IsHiddenFromAI() || Player->IsDead());
	}
}

AGuardNPC::AGuardNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	// An AI controller possesses the guard both when it is placed in the level and when it is spawned
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AGuardAIController::StaticClass();

	// The body turns towards where it walks; the tasks turn it to the player themselves
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	// Run speed: the jog of the pistol blend space is authored for 400 cm/s
	Movement->MaxWalkSpeed = 400.f;

	// Path following must drive the movement through acceleration: the Animation Blueprints switch from
	// Idle to Walk / Run only when the current acceleration is not zero (ShouldMove), otherwise the guard slides
	Movement->GetNavMovementProperties()->bUseAccelerationForPaths = true;

	// The mesh stands with its feet at the bottom of the capsule, facing forward
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight), FRotator(0.f, -90.f, 0.f));

	// The revolver is a separate asset attached to the skeleton's hand socket
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(GetMesh(), WeaponSocketName);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Health bar with the guard's own look (a name plate and segments)
	HealthBar = CreateDefaultSubobject<UEnemyHealthBarComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetCapsuleComponent());
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight + 45.f));
	HealthBar->SetWidgetClass(UGuardHealthBarWidget::StaticClass());
	HealthBar->SetDrawSize(FVector2D(260.f, 62.f));

	// Perception: the guard registers other actors by sight
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 2500.f;
	SightConfig->LoseSightRadius = 3000.f;
	SightConfig->PeripheralVisionAngleDegrees = 70.f;
	// Only enemies (PlayersTeam) are reported, so the handler does not have to check who it saw
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	PerceptionComponent->ConfigureSense(*SightConfig);
	PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AGuardNPC::BeginPlay()
{
	Super::BeginPlay();

	// Run speed comes from the movement component, the walk is a part of it
	RunSpeed = GetCharacterMovement()->MaxWalkSpeed;
	WalkSpeed = RunSpeed * WalkSpeedFraction;
	Walk();

	PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AGuardNPC::OnActorPerceived);
}

void AGuardNPC::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	GuardController = Cast<AGuardAIController>(NewController);
}

UBlackboardComponent* AGuardNPC::GetBlackboard() const
{
	return GuardController ? GuardController->GetBlackboardComponent() : nullptr;
}

ETeamAttitude::Type AGuardNPC::GetTeamAttitudeTowards(const AActor& Other) const
{
	// Hostile if the noticed actor implements the team interface and belongs to PlayersTeam
	const IGenericTeamAgentInterface* OtherTeamAgent = Cast<const IGenericTeamAgentInterface>(&Other);
	return OtherTeamAgent && OtherTeamAgent->GetGenericTeamId().GetId() == PlayersTeam
		? ETeamAttitude::Hostile
		: ETeamAttitude::Neutral;
}

void AGuardNPC::OnActorPerceived(AActor* Actor, FAIStimulus Stimulus)
{
	if (bIsDead || !Actor) { return; }

	// Remember who the sight sense reported: Tick keeps checking this actor (it may step into a blind zone)
	SightTarget = Actor;
	UpdateLineOfSight();
}

bool AGuardNPC::CanSee(AActor* Actor) const
{
	if (!Actor || IsHiddenFromGuards(Actor)) { return false; }

	const FActorPerceptionInfo* Info = PerceptionComponent->GetActorInfo(*Actor);
	const FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
	return Info && Info->LastSensedStimuli.IsValidIndex(SightID) && Info->LastSensedStimuli[SightID].IsActive();
}

void AGuardNPC::UpdateLineOfSight()
{
	AActor* Target = SightTarget.Get();
	const bool bSeesTarget = CanSee(Target);

	if (bSeesTarget)
	{
		PerceivedActor = Target;
		TargetLostElapsed = 0.f;
	}
	if (bSeesTarget == bHasLineOfSight) { return; }

	bHasLineOfSight = bSeesTarget;

	UBlackboardComponent* Blackboard = GetBlackboard();
	if (!Blackboard) { return; }

	Blackboard->SetValueAsBool(GuardController->GetHasLineOfSightKey(), bHasLineOfSight);
	if (bHasLineOfSight)
	{
		LastLocationElapsed = 0.f;
		Blackboard->SetValueAsObject(GuardController->GetDetectedEnemyKey(), PerceivedActor);
		Blackboard->SetValueAsVector(GuardController->GetLastKnownLocationKey(), PerceivedActor->GetActorLocation());
	}
}

void AGuardNPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDead) { return; }

	// The perception reports only changes; a blind zone can hide the player without one
	UpdateLineOfSight();
	UpdateWeaponRaise(DeltaTime);

	UBlackboardComponent* Blackboard = GetBlackboard();
	if (!Blackboard) { return; }

	// While the player is visible, refresh where they are
	if (bHasLineOfSight && PerceivedActor && (LastLocationElapsed += DeltaTime) >= LastLocationUpdateTime)
	{
		LastLocationElapsed = 0.f;
		Blackboard->SetValueAsVector(GuardController->GetLastKnownLocationKey(), PerceivedActor->GetActorLocation());
	}

	// Not seen for TargetLostTime: forget the player and go back to patrolling
	if (!bHasLineOfSight && PerceivedActor && (TargetLostElapsed += DeltaTime) >= TargetLostTime)
	{
		PerceivedActor = nullptr;
		TargetLostElapsed = 0.f;
		Blackboard->ClearValue(GuardController->GetDetectedEnemyKey());
	}
}

void AGuardNPC::UpdateWeaponRaise(float DeltaTime)
{
	if (bHasLineOfSight && PerceivedActor)
	{
		const float Distance = FVector::Dist(GetActorLocation(), PerceivedActor->GetActorLocation());
		if (Distance <= WeaponEffectiveRange * RaiseRangeFactor)
		{
			bWeaponRaised = true;
		}
		else if (Distance > WeaponEffectiveRange * LowerRangeFactor)
		{
			bWeaponRaised = false;
		}
	}
	else
	{
		bWeaponRaised = false;
	}

	// Constant speed: a full raise or lower takes RaiseTime; ABP_Guard blends the aimed upper body by this weight
	WeaponRaiseAlpha = FMath::FInterpConstantTo(WeaponRaiseAlpha, bWeaponRaised ? 1.f : 0.f, DeltaTime, 1.f / RaiseTime);
}

// The animation follows the speed by itself (ABP_Guard blend spaces), so Walk / Run only change the speed
void AGuardNPC::Walk()
{
	bIsRunning = false;
	ApplyMovementSpeed();
}

void AGuardNPC::Run()
{
	bIsRunning = true;
	ApplyMovementSpeed();
}

void AGuardNPC::SetSpeedMultiplier(float Multiplier)
{
	SpeedMultiplier = FMath::Max(Multiplier, 0.05f);
	ApplyMovementSpeed();
}

void AGuardNPC::ApplyMovementSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = (bIsRunning ? RunSpeed : WalkSpeed) * SpeedMultiplier;
}

bool AGuardNPC::FireAt(AActor* Target)
{
	UWorld* World = GetWorld();
	if (bIsDead || !Target || !World) { return false; }

	// Face the target: it may keep moving
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	SetActorRotation(FRotator(0.f, ToTarget.Rotation().Yaw, 0.f));

	// No shots with a lowered revolver: wait until the raise is finished
	if (!IsWeaponReady()) { return false; }

	if (FireMontage)
	{
		PlayAnimMontage(FireMontage);
	}

	// Aim at the chest with a small random spread
	const FVector Muzzle = WeaponMesh->GetComponentTransform().TransformPosition(MuzzleOffset);
	const FVector AimPoint = Target->GetActorLocation() + FVector(0.f, 0.f, 20.f);
	const FVector Direction = FMath::VRandCone((AimPoint - Muzzle).GetSafeNormal(), FMath::DegreesToRadians(WeaponSpread));
	const FVector TraceEnd = Muzzle + Direction * WeaponEffectiveRange * 2.f;

	// The same channel as the player's bullets: blocked by walls and characters, ignored by triggers
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardShot), false, this);
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Muzzle, TraceEnd, ECC_Projectile, Params);
	const FVector ShotEnd = bHit ? Hit.ImpactPoint : TraceEnd;

	if (MuzzleEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MuzzleEffect, Muzzle, Direction.Rotation());
	}
	if (bHit && ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	}
	// Short tracer, like the trail of the player's bullets
	DrawDebugLine(World, Muzzle, ShotEnd, FColor(255, 220, 120), false, 0.08f, 0, 1.5f);

	if (bHit && Hit.GetActor() == Target)
	{
		UGameplayStatics::ApplyPointDamage(Target, WeaponDamage, Direction, Hit, GetController(), this, UDamageType::StaticClass());
		return true;
	}
	return false;
}

void AGuardNPC::OnShotFired(AActor* Shooter, const FVector& From, const FVector& To)
{
	if (bIsDead || bIsDodging) { return; }

	// Does the shot pass close to the guard? (the line is extended a little past the aim point)
	const FVector Center = GetActorLocation();
	const FVector End = To + (To - From).GetSafeNormal() * 200.f;
	if (FVector::Dist(FMath::ClosestPointOnSegment(Center, From, End), Center) > ShotNoticeRadius) { return; }

	UBlackboardComponent* Blackboard = GetBlackboard();
	if (!Blackboard) { return; }

	// The behaviour tree decides whether the dodge succeeds (see UBTTask_Dodge)
	Blackboard->SetValueAsVector(GuardController->GetShotOriginKey(), From);
	Blackboard->SetValueAsBool(GuardController->GetIncomingShotKey(), true);

	// A shot gives the shooter away even if the guard did not see them
	if (!bHasLineOfSight && Shooter && !IsHiddenFromGuards(Shooter))
	{
		PerceivedActor = Shooter;
		TargetLostElapsed = 0.f;
		Blackboard->SetValueAsObject(GuardController->GetDetectedEnemyKey(), Shooter);
		Blackboard->SetValueAsVector(GuardController->GetLastKnownLocationKey(), Shooter->GetActorLocation());
	}
}

bool AGuardNPC::CanDodgeTo(float Side) const
{
	UWorld* World = GetWorld();
	if (!World) { return false; }

	const FVector Start = GetActorLocation();
	const FVector End = Start + GetActorRightVector() * FMath::Sign(Side) * DodgeClearance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardDodge), false, this);

	// No wall or exhibit on the way...
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(35.f), Params))
	{
		return false;
	}
	// ...and a floor to land on
	return World->LineTraceSingleByChannel(Hit, End, End - FVector(0.f, 0.f, 250.f), ECC_Visibility, Params);
}

void AGuardNPC::Dodge(float Side)
{
	if (bIsDead || bIsDodging) { return; }

	bIsDodging = true;

	// Keep facing the shooter during the jump, so it is a step to the side
	GetCharacterMovement()->bOrientRotationToMovement = false;
	LaunchCharacter(GetActorRightVector() * FMath::Sign(Side) * DodgeSpeed + FVector(0.f, 0.f, 200.f), true, true);

	GetWorldTimerManager().SetTimer(DodgeTimer, this, &AGuardNPC::FinishDodge, DodgeDuration, false);
}

void AGuardNPC::FinishDodge()
{
	bIsDodging = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
}

float AGuardNPC::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// The bullet flies past while the guard jumps aside
	if (bIsDead || bIsDodging) { return 0.f; }

	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Applied <= 0.f) { return 0.f; }

	// A hit tells the guard where the shooter is
	APawn* Shooter = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	UBlackboardComponent* Blackboard = GetBlackboard();
	if (Blackboard && Shooter && !IsHiddenFromGuards(Shooter))
	{
		PerceivedActor = Shooter;
		TargetLostElapsed = 0.f;
		Blackboard->SetValueAsObject(GuardController->GetDetectedEnemyKey(), Shooter);
		Blackboard->SetValueAsVector(GuardController->GetLastKnownLocationKey(), Shooter->GetActorLocation());
	}

	if (HealthBar->ApplyDamage(Applied))
	{
		Die();
	}
	return Applied;
}

void AGuardNPC::Die()
{
	bIsDead = true;
	bIsDodging = false;
	GetWorldTimerManager().ClearTimer(DodgeTimer);

	// Stop thinking: the behaviour tree and the perception are switched off, the controller leaves
	if (GuardController && GuardController->GetBrainComponent())
	{
		GuardController->GetBrainComponent()->StopLogic(TEXT("Dead"));
	}
	PerceptionComponent->Deactivate();
	DetachFromControllerPendingDestroy();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBar->SetVisibility(false);

	// The body falls as a ragdoll (physics asset of the mannequin)
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	SetLifeSpan(CorpseLifeSpan);
}
