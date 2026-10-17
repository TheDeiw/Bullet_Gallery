#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Teleporter.generated.h"

class APlayerCharacter;
class UArrowComponent;
class UBoxComponent;
class UPointLightComponent;
class USoundBase;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 * Teleport pad. When the player steps into the trigger box, they are moved to the exit point
 * of the linked Destination pad, which may be far away on the map. Two pads that point at
 * each other make a two-way link. A player who has just arrived is ignored by the destination
 * pad until they step off it, so they are not sent straight back.
 */
UCLASS()
class BULLET_GALLERY_API ATeleporter : public AActor
{
	GENERATED_BODY()

public:
	ATeleporter();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** World transform where an arriving player is placed (feet on the floor, facing the arrow) */
	FTransform GetExitTransform() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnTriggerEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void TeleportPlayer(APlayerCharacter* Player);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Volume that starts the teleport */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Trigger;

	/** Visible platform on the floor (the mesh is chosen in the Blueprint) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Light;

	/** Caption above the pad with the name of the destination */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> Label;

	/** Where the arriving player appears and which way they look */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> ExitPoint;

	/** Pad the player is sent to */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Teleport")
	TObjectPtr<ATeleporter> Destination;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	FText LabelText = NSLOCTEXT("Teleporter", "Label", "Teleport");

	/** Short black fade on arrival, seconds (0 = no fade) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport", meta = (ClampMin = 0))
	float ArrivalFadeTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teleport")
	TObjectPtr<USoundBase> TeleportSound;

private:
	/** Players that have just arrived here; they are ignored until they leave the trigger */
	TSet<TWeakObjectPtr<AActor>> ArrivedActors;
};
