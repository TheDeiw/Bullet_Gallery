#include "EnemyHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UEnemyHealthBarWidget::RebuildWidget()
{
	// The Widget Blueprint designer is empty: build the frame and the bar in code
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return Super::RebuildWidget();
}

void UEnemyHealthBarWidget::BuildDefaultLayout()
{
	Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	if (FrameTexture)
	{
		// Box drawing: corners and edges keep their size, only the middle stretches
		FSlateBrush Brush;
		Brush.SetResourceObject(FrameTexture);
		Brush.ImageSize = FVector2D(FrameTexture->GetSizeX(), FrameTexture->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FrameTextureMargin;
		Frame->SetBrush(Brush);
	}
	else
	{
		// Dark translucent plate with a light outline
		Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f), 6.f,
			FLinearColor(0.95f, 0.9f, 0.75f, 1.f), 2.f));
	}
	Frame->SetPadding(FramePadding);
	WidgetTree->RootWidget = Frame;

	HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	FProgressBarStyle Style;
	Style.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.02f, 0.02f, 0.9f), 3.f));
	// White fill: the actual colour comes from FillColorAndOpacity
	Style.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.f));
	Style.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.f));
	HealthProgressBar->SetWidgetStyle(Style);
	HealthProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);

	if (UBorderSlot* BarSlot = Cast<UBorderSlot>(Frame->SetContent(HealthProgressBar)))
	{
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UEnemyHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyValue();
}

void UEnemyHealthBarWidget::SetNewHealthValue(float Percent)
{
	CurrentPercent = FMath::Clamp(Percent, 0.f, 1.f);
	ApplyValue();
	OnHealthValueChanged(CurrentPercent);
}

void UEnemyHealthBarWidget::ApplyValue()
{
	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(CurrentPercent);
		HealthProgressBar->SetFillColorAndOpacity(GetColorForPercent(CurrentPercent));
	}
}

FLinearColor UEnemyHealthBarWidget::GetColorForPercent(float Percent) const
{
	// Upper half: green -> yellow, lower half: yellow -> red
	const float P = FMath::Clamp(Percent, 0.f, 1.f);
	return P >= 0.5f
		? FMath::Lerp(HalfHealthColor, FullHealthColor, (P - 0.5f) * 2.f)
		: FMath::Lerp(LowHealthColor, HalfHealthColor, P * 2.f);
}
