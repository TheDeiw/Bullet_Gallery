#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MuseumProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class UMaterialInterface;
class USoundBase;

/**
 * Bullet that flies to the point under the crosshair along a partly random spiral.
 * The curve is an offset added to the straight muzzle-to-target line; the offset is
 * multiplied by an envelope that is zero at both ends, so the bullet always lands on the target point.
 * Lab 6: the offset is switched off in Tick (commented out), so the bullets fly straight.
 */
UCLASS()
class BULLET_GALLERY_API AMuseumProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMuseumProjectile();

	virtual void Tick(float DeltaTime) override;

	/** Starts the flight from the current location to the point under the crosshair */
	void Launch(const FVector& InTargetPoint);

	/** Perks make the player's bullets hit harder: 1 = the Blueprint's damage */
	void SetDamageScale(float Scale) { DamageScale = Scale; }

protected:
	/** Sideways offset from the straight line at path fraction S (0 = muzzle, 1 = target) */
	FVector ComputeOffset(float S) const;

	/** Shows the hit (effect, bullet hole, sound, impulse, damage) and removes the bullet */
	void HandleImpact(const FHitResult& Hit, const FVector& FlightDirection);

	/** Sphere used for collision; the root of the actor */
	UPROPERTY(VisibleDefaultsOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleDefaultsOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	/** Flight speed along the path, cm/s */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float Speed = 2500.f;

	/** Largest spiral radius, cm */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float MaxSpiralRadius = 40.f;

	/** Spiral radius as a fraction of the shot distance, so close shots stay tight */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float RadiusPerDistance = 0.04f;

	/** Number of spiral turns per shot is picked at random from this range */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	FVector2D TurnsRange = FVector2D(1.5f, 3.5f);

	/** How strongly the radius pulses (0 = constant radius) */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float RadiusPulseAmplitude = 0.35f;

	/** Radius pulses per flight */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float RadiusPulseFrequency = 3.f;

	/** Random drift strength as a fraction of the spiral radius */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float NoiseAmplitude = 0.6f;

	/** How quickly the random drift changes along the path */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float NoiseFrequency = 4.f;

	/** The bullet is removed after flying this far without hitting anything (it left the scene) */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float MaxFlightDistance = 50000.f;

	/** Draws the flown path as a line for a short time (Lab 6: the drawing is commented out in Tick) */
	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	bool bDrawTrail = true;

	UPROPERTY(EditDefaultsOnly, Category = "Trajectory")
	float TrailLifeTime = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	/** Bullet hole left on the surface */
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TObjectPtr<UMaterialInterface> ImpactDecal;

	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	FVector DecalSize = FVector(6.f, 8.f, 8.f);

	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	float DecalLifeSpan = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	TObjectPtr<USoundBase> ImpactSound;

	/** Push applied to physics objects that are hit */
	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	float ImpulseStrength = 30000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Impact")
	float Damage = 25.f;

private:
	float DamageScale = 1.f;
	FVector StartPoint = FVector::ZeroVector;
	FVector TargetPoint = FVector::ZeroVector;

	/** Two axes perpendicular to the flight line; the spiral is drawn in their plane */
	FVector AxisRight = FVector::RightVector;
	FVector AxisUp = FVector::UpVector;

	float PathLength = 0.f;
	float Traveled = 0.f;
	float SpiralRadius = 0.f;

	// Values picked at random for every shot
	float Turns = 2.f;
	float Phase = 0.f;
	float SpinDirection = 1.f;
	float NoiseSeed = 0.f;

	bool bLaunched = false;
};
