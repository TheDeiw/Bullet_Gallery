#include "GuardHealthBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UGuardHealthBarWidget::BuildDefaultLayout()
{
	// Dark plate with a golden outline: the guard's badge
	Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.02f, 0.03f, 0.08f, 0.75f), 4.f, NameColor, 1.5f));
	Frame->SetPadding(FMargin(6.f, 3.f));
	WidgetTree->RootWidget = Frame;

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Rows"));
	Frame->SetContent(Rows);

	// Name plate
	UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	NameText->SetText(GuardName);
	NameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	NameText->SetColorAndOpacity(FSlateColor(NameColor));
	NameText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* NameSlot = Rows->AddChildToVerticalBox(NameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	}

	// Row of segments and the health number
	UHorizontalBox* BarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BarRow"));
	if (UVerticalBoxSlot* RowSlot = Rows->AddChildToVerticalBox(BarRow))
	{
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Segments.Reset();
	for (int32 i = 0; i < SegmentCount; ++i)
	{
		UBorder* Segment = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Segment->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 2.f));
		if (UHorizontalBoxSlot* SegmentSlot = BarRow->AddChildToHorizontalBox(Segment))
		{
			SegmentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SegmentSlot->SetPadding(FMargin(1.5f, 1.f));
		}
		Segments.Add(Segment);
	}

	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	HealthText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	if (UHorizontalBoxSlot* TextSlot = BarRow->AddChildToHorizontalBox(HealthText))
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	}
}

void UGuardHealthBarWidget::ApplyValue()
{
	// A segment is lit while health covers at least a part of it
	const int32 Lit = FMath::CeilToInt(CurrentPercent * Segments.Num());
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		if (Segments[i])
		{
			Segments[i]->SetBrushColor(i < Lit ? SegmentColor : EmptySegmentColor);
		}
	}

	if (HealthText)
	{
		HealthText->SetText(FText::AsNumber(FMath::RoundToInt(CurrentPercent * MaxHealthShown)));
	}
}
