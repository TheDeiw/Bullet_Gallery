#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PickupBase.generated.h"

class APlayerCharacter;
class UPointLightComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Base class for the items the player picks up by walking into them (medkit, ammo box).
 * The item spins and bobs in place so it is easy to notice. When the player touches the
 * sphere, ApplyPickup() decides whether the item is used (for example a medkit is ignored
 * at full health); a used item disappears and can come back after RespawnTime.
 */
UCLASS(Abstract)
class BULLET_GALLERY_API APickupBase : public AActor
{
	GENERATED_BODY()

public:
	APickupBase();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	/** Applies the item to the player. Returns false if the player does not need it now */
	UFUNCTION(BlueprintNativeEvent, Category = "Pickup")
	bool ApplyPickup(APlayerCharacter* Player);

	/** Text shown on the HUD after a pickup, e.g. "+50 health" */
	UFUNCTION(BlueprintNativeEvent, Category = "Pickup")
	FText GetPickupMessage() const;

	/** Called after a successful pickup; a Blueprint can add effects */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
	void OnPickedUp(APlayerCharacter* Player);

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Hides the item and turns its trigger off (true) or brings it back (false) */
	void SetPickupHidden(bool bHide);
	void Respawn();

	/** Trigger the player walks into */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	/** Visible model; it only spins, collisions are handled by the sphere */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Soft coloured light under the item so it is visible in dark halls */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	/** Spin speed around the vertical axis, degrees per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Animation")
	float RotationSpeed = 90.f;

	/** Height of the up-and-down motion, cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Animation")
	float BobAmplitude = 6.f;

	/** Up-and-down cycles per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup|Animation")
	float BobFrequency = 0.6f;

	/** Seconds until a used item appears again; 0 = it is removed for good */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = 0))
	float RespawnTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
	TObjectPtr<USoundBase> PickupSound;

private:
	FVector MeshBaseLocation = FVector::ZeroVector;
	float RunningTime = 0.f;
	FTimerHandle RespawnTimer;
};
