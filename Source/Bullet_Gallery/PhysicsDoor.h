#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicsDoor.generated.h"

class UPhysicsConstraintComponent;
class UStaticMeshComponent;

/**
 * Swing door driven by physics. The leaf is a simulated body held by a hinge
 * (Physics Constraint): it can only turn around the vertical axis within OpenAngle
 * in both directions. The player opens it by walking into it (the character movement
 * pushes physics bodies) or by shooting it; a weak angular spring closes it again.
 * The actor origin is the bottom of the hinge line; the leaf extends along +X.
 */
UCLASS()
class BULLET_GALLERY_API APhysicsDoor : public AActor
{
	GENERATED_BODY()

public:
	APhysicsDoor();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	/** Sizes the leaf and the handle from DoorSize (the meshes are 100 cm cubes) */
	void UpdateDoorLayout();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** The moving leaf */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Door;

	/** Decoration on both sides of the leaf, moves with it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Handle;

	/** Hinge that joins the leaf to the world */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicsConstraintComponent> Hinge;

	/** Width (X), thickness (Y) and height (Z) of the leaf, cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door")
	FVector DoorSize = FVector(120.f, 8.f, 240.f);

	/** How far the leaf can swing to each side, degrees */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 5, ClampMax = 175))
	float OpenAngle = 100.f;

	/** Heavier than what the player can lift, so it is pushed rather than carried */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 1))
	float DoorMass = 60.f;

	/** Strength of the spring that turns the leaf back to closed (0 = it stays where it was pushed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 0))
	float CloseStrength = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 0))
	float CloseDamping = 10.f;
};
