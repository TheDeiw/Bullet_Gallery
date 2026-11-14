#include "MuseumHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

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
	TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	// A dark shadow keeps the text readable in front of bright walls
	TextBlock->SetShadowOffset(FVector2D(1.f, 1.f));
	TextBlock->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
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
		FSlateBrush Brush;
		Brush.SetResourceObject(DamageOverlayTexture);
		Brush.ImageSize = FVector2D(DamageOverlayTexture->GetSizeX(), DamageOverlayTexture->GetSizeY());
		DamageOverlay->SetBrush(Brush);
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

	// Two short blocks in the top-left corner, no background
	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
	Rows->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* RowsSlot = Root->AddChildToCanvas(Rows))
	{
		// Bottom-left corner: the top of the screen belongs to the wave counter and the score
		RowsSlot->SetAnchors(FAnchors(0.f, 1.f));
		RowsSlot->SetAlignment(FVector2D(0.f, 1.f));
		RowsSlot->SetPosition(FVector2D(40.f, -34.f));
		RowsSlot->SetAutoSize(true);
	}

	// Health: the title and a flat red line
	Rows->AddChildToVerticalBox(MakeText(TEXT("HealthLabel"), HealthLabel, 14));

	USizeBox* HealthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthBarSize"));
	HealthBox->SetWidthOverride(FMath::Max(HealthBarSize.X, 300.f));
	HealthBox->SetHeightOverride(FMath::Max(HealthBarSize.Y, 14.f));

	HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	FProgressBarStyle HealthStyle;
	HealthStyle.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.45f)));
	// White fill: the colour comes from FillColorAndOpacity
	HealthStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
	HealthStyle.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
	HealthProgressBar->SetWidgetStyle(HealthStyle);
	HealthProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	HealthBox->SetContent(HealthProgressBar);
	if (UVerticalBoxSlot* BarSlot = Rows->AddChildToVerticalBox(HealthBox))
	{
		BarSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 18.f));
	}

	// Bullets: the title and "in magazine / in reserve"
	Rows->AddChildToVerticalBox(MakeText(TEXT("AmmoLabel"), AmmoLabel, 14));
	AmmoText = MakeText(TEXT("AmmoText"), FText::GetEmpty(), 34);
	Rows->AddChildToVerticalBox(AmmoText);

	// Shown only while the pistol is being reloaded
	ReloadText = MakeText(TEXT("ReloadText"), ReloadLabel, 18);
	ReloadText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.85f, 0.54f)));
	ReloadText->SetVisibility(ESlateVisibility::Collapsed);
	Rows->AddChildToVerticalBox(ReloadText);

	// Hint a little below the centre of the screen (the crosshair)
	HintText = MakeText(TEXT("HintText"), FText::GetEmpty(), 14);
	HintText->SetJustification(ETextJustify::Center);
	HintText->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(HintText))
	{
		HintSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		HintSlot->SetAlignment(FVector2D(0.5f, 0.f));
		HintSlot->SetPosition(FVector2D(0.f, 40.f));
		HintSlot->SetAutoSize(true);
	}

	// Pickup and teleport messages in the upper third of the screen
	MessageText = MakeText(TEXT("MessageText"), FText::GetEmpty(), 22);
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetVisibility(ESlateVisibility::HitTestInvisible);
	MessageText->SetRenderOpacity(0.f);
	if (UCanvasPanelSlot* MessageSlot = Root->AddChildToCanvas(MessageText))
	{
		MessageSlot->SetAnchors(FAnchors(0.5f, 0.3f));
		MessageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MessageSlot->SetPosition(FVector2D(0.f, 0.f));
		MessageSlot->SetAutoSize(true);
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

	// The message stays for MessageDuration, then fades out
	if (MessageOpacity > 0.f)
	{
		MessageTimeLeft -= InDeltaTime;
		if (MessageTimeLeft < 0.f)
		{
			MessageOpacity = FMath::Max(MessageOpacity - MessageFadeSpeed * InDeltaTime, 0.f);
		}
		if (MessageText)
		{
			MessageText->SetRenderOpacity(MessageOpacity);
		}
	}
}

void UMuseumHUDWidget::SetInteractionHint(const FText& Hint)
{
	CurrentHint = Hint;
	ApplyValues();
}

void UMuseumHUDWidget::ShowMessage(const FText& Message)
{
	MessageTimeLeft = MessageDuration;
	MessageOpacity = 1.f;
	if (MessageText)
	{
		MessageText->SetText(Message);
		MessageText->SetRenderOpacity(MessageOpacity);
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
	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(CurrentMaxHealth > 0.f ? CurrentHealth / CurrentMaxHealth : 0.f);
		HealthProgressBar->SetFillColorAndOpacity(HealthColor);
	}
	if (AmmoText)
	{
		AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmmo, CurrentReserve)));
	}
	if (ReloadText)
	{
		ReloadText->SetVisibility(bIsReloading ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (HintText)
	{
		HintText->SetText(CurrentHint);
		HintText->SetVisibility(CurrentHint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
