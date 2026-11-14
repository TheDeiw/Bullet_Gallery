#include "MuseumProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

AMuseumProjectile::AMuseumProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	CollisionComponent->InitSphereRadius(3.f);
	CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
	RootComponent = CollisionComponent;

	// The mesh is only the visible part; collisions are handled by the sphere
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetCastShadow(false);

	// No InitialLifeSpan on purpose: the bullet disappears only on a hit or after leaving the scene
}

void AMuseumProjectile::Launch(const FVector& InTargetPoint)
{
	StartPoint = GetActorLocation();
	TargetPoint = InTargetPoint;
	PathLength = FMath::Max(FVector::Dist(StartPoint, TargetPoint), 1.f);
	Traveled = 0.f;

	// Build two axes perpendicular to the flight direction
	const FVector Direction = (TargetPoint - StartPoint).GetSafeNormal();
	const FVector Reference = FMath::Abs(Direction.Z) > 0.95f ? FVector::ForwardVector : FVector::UpVector;
	AxisRight = FVector::CrossProduct(Reference, Direction).GetSafeNormal();
	AxisUp = FVector::CrossProduct(Direction, AxisRight);

	// Close shots get a small spiral so the bullet does not clip nearby walls
	SpiralRadius = FMath::Min(MaxSpiralRadius, PathLength * RadiusPerDistance);

	// Random part: every shot gets its own spiral
	Turns = FMath::FRandRange(TurnsRange.X, TurnsRange.Y);
	Phase = FMath::FRandRange(0.f, 2.f * PI);
	SpinDirection = FMath::RandBool() ? 1.f : -1.f;
	NoiseSeed = FMath::FRandRange(0.f, 1000.f);

	// Never collide with the shooter
	if (AActor* Shooter = GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(Shooter, true);
	}

	bLaunched = true;
}

FVector AMuseumProjectile::ComputeOffset(float S) const
{
	// Envelope: 0 at the muzzle and at the target (with zero slope), 1 in the middle of the path
	const float Envelope = FMath::Square(FMath::Sin(PI * S));

	// Spiral around the flight line with a pulsing radius
	const float Angle = SpinDirection * 2.f * PI * Turns * S + Phase;
	const float Radius = SpiralRadius * (1.f + RadiusPulseAmplitude * FMath::Sin(2.f * PI * RadiusPulseFrequency * S + Phase));
	const FVector Spiral = Radius * (FMath::Cos(Angle) * AxisRight + FMath::Sin(Angle) * AxisUp);

	// Smooth random drift (Perlin noise), different for every shot
	const float NoiseRight = FMath::PerlinNoise1D(NoiseSeed + S * NoiseFrequency);
	const float NoiseUp = FMath::PerlinNoise1D(NoiseSeed + 137.f + S * NoiseFrequency);
	const FVector Drift = SpiralRadius * NoiseAmplitude * (NoiseRight * AxisRight + NoiseUp * AxisUp);

	return Envelope * (Spiral + Drift);
}

void AMuseumProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bLaunched) { return; }

	const FVector OldLocation = GetActorLocation();
	Traveled += Speed * DeltaTime;

	FVector NewLocation;
	if (Traveled <= PathLength)
	{
		const float S = Traveled / PathLength;
		// Random spread disabled for Lab 6; uncomment to bring it back
		NewLocation = FMath::Lerp(StartPoint, TargetPoint, S) /* + ComputeOffset(S) */;
	}
	else
	{
		// Nothing was at the target point (sky or a moved object): keep flying straight
		const FVector Direction = (TargetPoint - StartPoint).GetSafeNormal();
		NewLocation = TargetPoint + Direction * (Traveled - PathLength);
	}

	const FVector FlightDirection = (NewLocation - OldLocation).GetSafeNormal();
	if (!FlightDirection.IsNearlyZero())
	{
		SetActorRotation(FlightDirection.Rotation());
	}

	// Sweep so that anything between the old and the new position is detected
	FHitResult Hit;
	SetActorLocation(NewLocation, true, &Hit);

	// Bullet trail disabled for Lab 6; uncomment to bring it back
	// if (bDrawTrail)
	// {
	// 	DrawDebugLine(GetWorld(), OldLocation, GetActorLocation(), FColor::Orange, false, TrailLifeTime, 0, 1.f);
	// }

	if (Hit.bBlockingHit)
	{
		HandleImpact(Hit, FlightDirection);
	}
	else if (Traveled > MaxFlightDistance)
	{
		// Left the scene without hitting anything
		Destroy();
	}
}

void AMuseumProjectile::HandleImpact(const FHitResult& Hit, const FVector& FlightDirection)
{
	if (ImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
	}

	if (ImpactDecal && Hit.GetComponent())
	{
		// Attached to the component so the hole moves together with a movable object
		UGameplayStatics::SpawnDecalAttached(ImpactDecal, DecalSize, Hit.GetComponent(), NAME_None,
			Hit.ImpactPoint, (-Hit.ImpactNormal).Rotation(), EAttachLocation::KeepWorldPosition, DecalLifeSpan);
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint);
	}

	if (UPrimitiveComponent* HitComponent = Hit.GetComponent())
	{
		if (HitComponent->IsSimulatingPhysics())
		{
			HitComponent->AddImpulseAtLocation(FlightDirection * ImpulseStrength, Hit.ImpactPoint);
		}
	}

	if (AActor* HitActor = Hit.GetActor())
	{
		UGameplayStatics::ApplyPointDamage(HitActor, Damage * DamageScale, FlightDirection, Hit,
			GetInstigatorController(), this, UDamageType::StaticClass());
	}

	Destroy();
}
