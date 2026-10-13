#include "MuseumHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** Brush that draws a texture stretched over the whole widget */
	FSlateBrush MakeImageBrush(UTexture2D* Texture)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Brush.DrawAs = ESlateBrushDrawType::Image;
		return Brush;
	}
}

TSharedRef<SWidget> UMuseumHUDWidget::RebuildWidget()
{
	// The Widget Blueprint designer is empty: build the HUD in code
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return Super::RebuildWidget();
}

UTextBlock* UMuseumHUDWidget::MakeText(FName Name, const FText& Text, int32 FontSize)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetText(Text);
	TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
	TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.92f, 0.85f)));
	// A dark shadow keeps the text readable in front of bright walls
	TextBlock->SetShadowOffset(FVector2D(1.f, 1.f));
	TextBlock->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
	return TextBlock;
}

void UMuseumHUDWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// Red frame over the whole screen; invisible until the player is hit
	DamageOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DamageOverlay"));
	if (DamageOverlayTexture)
	{
		DamageOverlay->SetBrush(MakeImageBrush(DamageOverlayTexture));
	}
	else
	{
		DamageOverlay->SetColorAndOpacity(FLinearColor(0.7f, 0.f, 0.f, 0.5f));
	}
	DamageOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	DamageOverlay->SetRenderOpacity(0.f);
	if (UCanvasPanelSlot* OverlaySlot = Root->AddChildToCanvas(DamageOverlay))
	{
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		OverlaySlot->SetOffsets(FMargin(0.f));
	}

	// Panel with both bars in the bottom-left corner
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatsPanel"));
	Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f), 10.f,
		FLinearColor(1.f, 1.f, 1.f, 0.25f), 1.f));
	Panel->SetPadding(FMargin(18.f, 12.f));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel))
	{
		PanelSlot->SetAnchors(FAnchors(0.f, 1.f));
		PanelSlot->SetAlignment(FVector2D(0.f, 1.f));
		PanelSlot->SetPosition(FVector2D(32.f, -32.f));
		PanelSlot->SetAutoSize(true);
	}

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
	Panel->SetContent(Rows);

	// Health: caption, then the bar with the numbers on top of it
	Rows->AddChildToVerticalBox(MakeText(TEXT("HealthLabel"), HealthLabel, 13));

	UOverlay* HealthRow = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HealthRow"));
	USizeBox* HealthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthBarSize"));
	HealthBox->SetWidthOverride(HealthBarSize.X);
	HealthBox->SetHeightOverride(HealthBarSize.Y);

	HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	FProgressBarStyle HealthStyle;
	HealthStyle.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.03f, 0.03f, 0.85f), 5.f,
		FLinearColor(1.f, 1.f, 1.f, 0.35f), 1.f));
	// White fill: the colour comes from FillColorAndOpacity
	HealthStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f));
	HealthStyle.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor::White, 5.f));
	HealthProgressBar->SetWidgetStyle(HealthStyle);
	HealthProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	HealthBox->SetContent(HealthProgressBar);
	HealthRow->AddChildToOverlay(HealthBox);

	HealthText = MakeText(TEXT("HealthText"), FText::GetEmpty(), 13);
	if (UOverlaySlot* TextSlot = HealthRow->AddChildToOverlay(HealthText))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* RowSlot = Rows->AddChildToVerticalBox(HealthRow))
	{
		RowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f));
	}

	// Ammo: caption with the counters, then the row of bullets
	UHorizontalBox* AmmoHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AmmoHeader"));
	if (UHorizontalBoxSlot* LabelSlot = AmmoHeader->AddChildToHorizontalBox(MakeText(TEXT("AmmoLabel"), AmmoLabel, 13)))
	{
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	AmmoText = MakeText(TEXT("AmmoText"), FText::GetEmpty(), 13);
	AmmoHeader->AddChildToHorizontalBox(AmmoText);
	Rows->AddChildToVerticalBox(AmmoHeader);

	USizeBox* AmmoBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AmmoBarSize"));
	AmmoBox->SetWidthOverride(AmmoBarSize.X);
	AmmoBox->SetHeightOverride(AmmoBarSize.Y);

	AmmoProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("AmmoProgressBar"));
	FProgressBarStyle AmmoStyle;
	if (AmmoFillTexture && AmmoBackgroundTexture)
	{
		// The fill image is clipped, not squeezed: Percent = 0.7 shows exactly 7 of 10 bullets
		AmmoStyle.SetBackgroundImage(MakeImageBrush(AmmoBackgroundTexture));
		AmmoStyle.SetFillImage(MakeImageBrush(AmmoFillTexture));
		AmmoStyle.SetMarqueeImage(MakeImageBrush(AmmoFillTexture));
	}
	else
	{
		AmmoStyle.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.1f, 0.08f, 0.03f, 0.85f), 5.f));
		AmmoStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.78f, 0.3f), 5.f));
		AmmoStyle.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.78f, 0.3f), 5.f));
	}
	AmmoProgressBar->SetWidgetStyle(AmmoStyle);
	AmmoProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	AmmoProgressBar->SetFillColorAndOpacity(FLinearColor::White);
	AmmoBox->SetContent(AmmoProgressBar);
	if (UVerticalBoxSlot* AmmoSlot = Rows->AddChildToVerticalBox(AmmoBox))
	{
		AmmoSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	}

	// Reload hint under the crosshair
	ReloadText = MakeText(TEXT("ReloadText"), ReloadLabel, 18);
	ReloadText->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* ReloadSlot = Root->AddChildToCanvas(ReloadText))
	{
		ReloadSlot->SetAnchors(FAnchors(0.5f, 0.58f));
		ReloadSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ReloadSlot->SetAutoSize(true);
	}
}

void UMuseumHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyValues();
}

void UMuseumHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	FlashOpacity = FMath::Max(FlashOpacity - FlashFadeSpeed * InDeltaTime, 0.f);

	if (DamageOverlay)
	{
		// With little health left the red frame does not fade out completely
		const bool bLowHealth = CurrentMaxHealth > 0.f && CurrentHealth / CurrentMaxHealth <= LowHealthFraction;
		DamageOverlay->SetRenderOpacity(FMath::Max(FlashOpacity, bLowHealth ? LowHealthOpacity : 0.f));
	}
}

void UMuseumHUDWidget::SetHealth(float Health, float MaxHealth)
{
	CurrentHealth = Health;
	CurrentMaxHealth = MaxHealth;
	ApplyValues();
}

void UMuseumHUDWidget::SetAmmo(int32 AmmoInMagazine, int32 MagazineSize, int32 ReserveAmmo)
{
	CurrentAmmo = AmmoInMagazine;
	CurrentMagazineSize = MagazineSize;
	CurrentReserve = ReserveAmmo;
	ApplyValues();
}

void UMuseumHUDWidget::SetReloading(bool bReloading)
{
	bIsReloading = bReloading;
	ApplyValues();
}

void UMuseumHUDWidget::PlayDamageFlash()
{
	FlashOpacity = MaxFlashOpacity;
}

void UMuseumHUDWidget::ApplyValues()
{
	const float HealthFraction = CurrentMaxHealth > 0.f ? CurrentHealth / CurrentMaxHealth : 0.f;

	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(HealthFraction);
		HealthProgressBar->SetFillColorAndOpacity(HealthColor);
	}
	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
			FMath::CeilToInt(CurrentHealth), FMath::RoundToInt(CurrentMaxHealth))));
	}
	if (AmmoProgressBar)
	{
		// One bullet = one step of 1 / MagazineSize
		AmmoProgressBar->SetPercent(CurrentMagazineSize > 0 ? float(CurrentAmmo) / CurrentMagazineSize : 0.f);
	}
	if (AmmoText)
	{
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d   +%d"),
			CurrentAmmo, CurrentMagazineSize, CurrentReserve)));
	}
	if (ReloadText)
	{
		ReloadText->SetVisibility(bIsReloading ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
