#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AutoSlidingDoor.generated.h"

class UBoxComponent;
class UMaterialInterface;
class USoundBase;
class UStaticMeshComponent;

/**
 * Automatic glass door. Two glass leaves fill the doorway; when a pawn (the player or an enemy)
 * walks into the trigger on either side, the leaves slide apart into the wall, and they slide
 * back once nobody is near. The leaves are moved kinematically, so they are not pushed by physics.
 * The actor origin is the middle of the doorway on the floor; the doorway runs along local Y.
 */
UCLASS()
class BULLET_GALLERY_API AAutoSlidingDoor : public AActor
{
	GENERATED_BODY()

public:
	AAutoSlidingDoor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** Opens the door while any pawn stands in the trigger */
	void UpdateTargetState();

	/** Places and sizes all parts from the door settings */
	void UpdateLayout();

	/** Moves the leaves for the current opening (0 = closed, 1 = open) */
	void ApplyOpenAmount(float Amount);

	UStaticMeshComponent* CreatePart(FName Name, USceneComponent* Parent, bool bBlocks);
	static void PlaceBox(UStaticMeshComponent* Part, const FVector& Center, const FVector& Size);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Volume on both sides of the doorway that opens the door */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Trigger;

	/** Moving parts of the left and right leaves */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LeftLeaf;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RightLeaf;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftGlass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightGlass;

	/** Metal edges of the leaves: the vertical bar where the leaves meet and the bottom rail */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftEdge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightEdge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LeftRail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> RightRail;

	/** Metal box above the doorway that hides the sliding mechanism */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Header;

	/** Fixed glass between the header and the ceiling */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Transom;

	/** Width of the opening, cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 50))
	float DoorwayWidth = 340.f;

	/** Height of the moving leaves, cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 100))
	float DoorHeight = 240.f;

	/** Top of the fixed glass above the door (the ceiling), cm above the floor; 0 = no transom */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 0))
	float TransomTop = 0.f;

	/** How far the trigger reaches in front of and behind the door, cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 50))
	float TriggerDepth = 220.f;

	/** Full open or close cycles per second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 0.1))
	float OpenSpeed = 1.6f;

	/** Part of a leaf that slides into the wall (a little stays visible as a frame) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door", meta = (ClampMin = 0.1, ClampMax = 1))
	float OpenFraction = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Look")
	TObjectPtr<UMaterialInterface> GlassMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Look")
	TObjectPtr<UMaterialInterface> FrameMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Sound")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Door|Sound")
	TObjectPtr<USoundBase> CloseSound;

private:
	/** Current opening, 0 = closed, 1 = fully open */
	float OpenAmount = 0.f;
	bool bWantsOpen = false;
};
