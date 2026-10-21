#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

class UBorder;
class UProgressBar;
class UTexture2D;

/**
 * Health bar drawn above an enemy (the "BP_DummyWidget" of the lab guide):
 * a Border frame with a ProgressBar inside. The fill colour goes from green through yellow to red
 * as health drops. Widgets named Frame / HealthProgressBar in the designer are bound automatically;
 * if the designer is empty, the same layout is built in code.
 */
UCLASS()
class BULLET_GALLERY_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the bar to Percent (0..1) and picks the colour for it */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetNewHealthValue(float Percent);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	/** Lets a Blueprint child add its own reaction to a health change */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnHealthValueChanged(float Percent);

	/** Colour of the bar for a given health fraction */
	UFUNCTION(BlueprintPure, Category = "Health")
	FLinearColor GetColorForPercent(float Percent) const;

	/** Frame around the bar: a Border keeps the edges of its image unscaled and stretches only the middle */
	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Frame;

	UPROPERTY(BlueprintReadOnly, Category = "Health", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor FullHealthColor = FLinearColor(0.1f, 0.85f, 0.2f);

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor HalfHealthColor = FLinearColor(1.f, 0.75f, 0.05f);

	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor LowHealthColor = FLinearColor(0.9f, 0.06f, 0.04f);

	/** Frame image with a transparent middle for the layout built in code; empty = a rounded outline */
	UPROPERTY(EditAnywhere, Category = "Style")
	TObjectPtr<UTexture2D> FrameTexture;

	/** Part of the frame image (0..0.5 of its size) that is never stretched: the corners and edges */
	UPROPERTY(EditAnywhere, Category = "Style")
	FMargin FrameTextureMargin = FMargin(0.25f);

	/** Space between the frame edge and the bar, pixels */
	UPROPERTY(EditAnywhere, Category = "Style")
	FMargin FramePadding = FMargin(6.f, 5.f);

	/** Creates Frame and HealthProgressBar when the Widget Blueprint designer has no widgets */
	virtual void BuildDefaultLayout();

	/** Shows CurrentPercent on the bar */
	virtual void ApplyValue();

	/** Kept so a value set before the widget is built is not lost */
	float CurrentPercent = 1.f;
};
