#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "EnemyHealthBarComponent.generated.h"

/**
 * 3D widget above an enemy (the "BP_DummyActorComponent" of the lab guide).
 * Keeps the enemy's health, counts the damage it takes and pushes the new value into
 * the health bar widget. The flat panel is turned towards the player's camera every frame,
 * so it can be read from any side.
 */
UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class BULLET_GALLERY_API UEnemyHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UEnemyHealthBarComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Subtracts damage and refreshes the bar; returns true when health reaches zero */
	UFUNCTION(BlueprintCallable, Category = "Health")
	bool ApplyDamage(float Amount);

	/** Sends the current health to the widget */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void UpdateHealthValue();

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

protected:
	virtual void BeginPlay() override;

	/** Turns the panel to face the player's camera */
	void FaceCamera();

	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = 1))
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	float Health = 100.f;
};
