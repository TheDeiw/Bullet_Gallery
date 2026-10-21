#pragma once

#include "CoreMinimal.h"
#include "EnemyHealthBarWidget.h"
#include "GuardHealthBarWidget.generated.h"

class UBorder;
class UTextBlock;

/**
 * Health bar of the museum guard. It looks different from the statues' bars (one bar from green to red)
 * and from the player's HUD: a name plate above a row of blue segments, one segment per tenth of health,
 * and the health number on the right. The layout is built in code.
 */
UCLASS()
class BULLET_GALLERY_API UGuardHealthBarWidget : public UEnemyHealthBarWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildDefaultLayout() override;
	virtual void ApplyValue() override;

	/** Text on the name plate */
	UPROPERTY(EditAnywhere, Category = "Style")
	FText GuardName = NSLOCTEXT("Guard", "Name", "NIGHT GUARD");

	/** Number of segments in the bar */
	UPROPERTY(EditAnywhere, Category = "Style", meta = (ClampMin = 2, ClampMax = 20))
	int32 SegmentCount = 10;

	/** Shown as a number next to the segments */
	UPROPERTY(EditAnywhere, Category = "Style")
	float MaxHealthShown = 100.f;

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor SegmentColor = FLinearColor(0.15f, 0.55f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor EmptySegmentColor = FLinearColor(0.05f, 0.07f, 0.12f, 0.85f);

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor NameColor = FLinearColor(1.f, 0.82f, 0.35f);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> Segments;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;
};
