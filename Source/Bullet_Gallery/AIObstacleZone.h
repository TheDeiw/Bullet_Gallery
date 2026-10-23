#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavAreas/NavArea.h"
#include "AIObstacleZone.generated.h"

class UBoxComponent;
class UNavModifierComponent;
class UStaticMeshComponent;

/** What the zone does */
UENUM(BlueprintType)
enum class EAIObstacleZoneType : uint8
{
	/** Poisonous fumes from the restoration chemicals: a guard inside moves slowly */
	PoisonSlow,
	/** Dark corner without lights: a player standing inside cannot be seen by the guards */
	BlindZone
};

/** Navigation area of the poison zone: it costs the same as a normal floor, so the guards do not go around it */
UCLASS()
class BULLET_GALLERY_API UNavArea_Poison : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_Poison();
};

/**
 * Obstacle for the AI guard made with a trigger volume.
 * PoisonSlow: a guard that enters is slowed down (SpeedMultiplier) until it leaves; the zone is also
 * marked on the navigation mesh with its own area (normal cost), so the path finding does not avoid it.
 * BlindZone: while the player stands inside, the guards' sight ignores them ("blind spot").
 */
UCLASS()
class BULLET_GALLERY_API AAIObstacleZone : public AActor
{
	GENERATED_BODY()

public:
	AAIObstacleZone();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Trigger volume */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<UBoxComponent> Trigger;

	/** Translucent box that shows the zone (fumes or shadow) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** Marks the zone on the navigation mesh (only used by PoisonSlow) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone")
	TObjectPtr<UNavModifierComponent> NavModifier;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	EAIObstacleZoneType ZoneType = EAIObstacleZoneType::PoisonSlow;

	/** Speed of a poisoned guard as a part of its normal speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone", meta = (ClampMin = 0.05, ClampMax = 1))
	float SlowMultiplier = 0.3f;

	/** Size of the zone (half of each side), cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone")
	FVector ZoneExtent = FVector(250.f, 250.f, 150.f);

	/** Material of the visual box, one per zone type */
	UPROPERTY(EditAnywhere, Category = "Zone")
	TObjectPtr<UMaterialInterface> PoisonMaterial;

	UPROPERTY(EditAnywhere, Category = "Zone")
	TObjectPtr<UMaterialInterface> BlindMaterial;
};
