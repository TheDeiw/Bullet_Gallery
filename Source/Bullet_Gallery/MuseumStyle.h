#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

/**
 * Look of the menus and the HUD: a dark "museum at night" palette with gold accents, and small factory functions
 * that build the repeated Slate pieces (buttons, sliders, option rows). Everything is built in code, so the UI has no
 * Blueprint or texture dependencies.
 */
namespace MuseumUI
{
	inline FLinearColor Hex(const TCHAR* Code, float Alpha = 1.f)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor::FromHex(Code));
		Color.A = Alpha;
		return Color;
	}

	namespace Colors
	{
		inline const FLinearColor Ink = Hex(TEXT("#06080c"));
		inline const FLinearColor Panel = Hex(TEXT("#0f1218"), 0.92f);
		inline const FLinearColor PanelHi = Hex(TEXT("#1b2130"), 0.95f);
		inline const FLinearColor Line = Hex(TEXT("#2b3140"));
		inline const FLinearColor Gold = Hex(TEXT("#d9a94b"));
		inline const FLinearColor GoldHi = Hex(TEXT("#f5d98a"));
		inline const FLinearColor Text = Hex(TEXT("#ece8df"));
		inline const FLinearColor Muted = Hex(TEXT("#8c919c"));
		inline const FLinearColor Red = Hex(TEXT("#d24c40"));
		inline const FLinearColor Green = Hex(TEXT("#62c07e"));
		inline const FLinearColor Cyan = Hex(TEXT("#5fb7de"));
		inline const FLinearColor Clear = FLinearColor(0.f, 0.f, 0.f, 0.f);
	}

	inline FSlateFontInfo Font(const TCHAR* Weight, int32 Size, int32 Spacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(Weight, Size);
		Info.LetterSpacing = Spacing;
		return Info;
	}

	/** Called for every click (true) or hover (false) on a menu control; set by the director to play a sound */
	inline TFunction<void(bool bClick)> UISound;

	inline const FSlateBrush* Solid(const FLinearColor& Color)
	{
		// Brushes must outlive the widgets that use them: keep one per colour for the whole run
		static TMap<uint32, TUniquePtr<FSlateColorBrush>> Cache;
		const uint32 Key = Color.ToFColor(true).DWColor();
		TUniquePtr<FSlateColorBrush>& Entry = Cache.FindOrAdd(Key);
		if (!Entry) { Entry = MakeUnique<FSlateColorBrush>(Color); }
		return Entry.Get();
	}

	inline const FSlateBrush* Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		static TMap<uint64, TUniquePtr<FSlateRoundedBoxBrush>> Cache;
		const uint64 Key = (uint64(Fill.ToFColor(true).DWColor()) << 32) ^ uint64(Outline.ToFColor(true).DWColor()) ^ (uint64(Radius * 4.f) << 8) ^ uint64(OutlineWidth * 4.f);
		TUniquePtr<FSlateRoundedBoxBrush>& Entry = Cache.FindOrAdd(Key);
		if (!Entry) { Entry = MakeUnique<FSlateRoundedBoxBrush>(Fill, Radius, Outline, OutlineWidth); }
		return Entry.Get();
	}

	inline TSharedRef<STextBlock> Label(const FText& Text, int32 Size, const FLinearColor& Color = Colors::Text, const TCHAR* Weight = TEXT("Regular"), int32 Spacing = 0)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font(Weight, Size, Spacing))
			.ColorAndOpacity(Color)
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	}

	inline const FButtonStyle& ButtonStyle(bool bPrimary)
	{
		static FButtonStyle Normal = FButtonStyle()
			.SetNormal(*Rounded(Hex(TEXT("#10141c"), 0.78f), 3.f, Colors::Line, 1.f))
			.SetHovered(*Rounded(Hex(TEXT("#1c2230"), 0.95f), 3.f, Colors::Gold, 1.5f))
			.SetPressed(*Rounded(Colors::Gold, 3.f, Colors::GoldHi, 1.5f))
			.SetNormalForeground(Colors::Text)
			.SetHoveredForeground(Colors::GoldHi)
			.SetPressedForeground(Colors::Ink)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		static FButtonStyle Primary = FButtonStyle()
			.SetNormal(*Rounded(Hex(TEXT("#d9a94b"), 0.95f), 3.f, Colors::GoldHi, 1.f))
			.SetHovered(*Rounded(Colors::GoldHi, 3.f, FLinearColor::White, 1.5f))
			.SetPressed(*Rounded(Hex(TEXT("#b9893a")), 3.f, Colors::GoldHi, 1.f))
			.SetNormalForeground(Colors::Ink)
			.SetHoveredForeground(Colors::Ink)
			.SetPressedForeground(Colors::Ink)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		return bPrimary ? Primary : Normal;
	}

	/** Wide menu button; OnClick runs after the click sound */
	inline TSharedRef<SWidget> Button(const FText& Text, TFunction<void()> OnClick, bool bPrimary = false, float Width = 340.f, float Height = 50.f, int32 FontSize = 18)
	{
		return SNew(SBox)
			.WidthOverride(Width)
			.HeightOverride(Height)
			[
				SNew(SButton)
				.ButtonStyle(&ButtonStyle(bPrimary))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnHovered_Lambda([]() { if (UISound) { UISound(false); } })
				.OnClicked_Lambda([OnClick]()
				{
					if (UISound) { UISound(true); }
					OnClick();
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(Text)
					.Font(Font(TEXT("Bold"), FontSize, 3))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	/** Small square "<" / ">" button for the option rows */
	inline TSharedRef<SWidget> ArrowButton(const TCHAR* Glyph, TFunction<void()> OnClick)
	{
		return SNew(SBox)
			.WidthOverride(38.f)
			.HeightOverride(34.f)
			[
				SNew(SButton)
				.ButtonStyle(&ButtonStyle(false))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnHovered_Lambda([]() { if (UISound) { UISound(false); } })
				.OnClicked_Lambda([OnClick]()
				{
					if (UISound) { UISound(true); }
					OnClick();
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(Glyph))
					.Font(Font(TEXT("Bold"), 18))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	/** One line of a settings page: the option name on the left, the control on the right */
	inline TSharedRef<SWidget> Row(const FText& Name, TSharedRef<SWidget> Control)
	{
		return SNew(SBox)
			.HeightOverride(41.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					Label(Name, 17, Colors::Text)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(330.f).VAlign(VAlign_Center)[Control]
				]
			];
	}

	/** "<  value  >" selector; GetText is polled every frame so it always shows the current value */
	inline TSharedRef<SWidget> Cycler(TFunction<FText()> GetText, TFunction<void()> OnPrev, TFunction<void()> OnNext)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ArrowButton(TEXT("<"), OnPrev)]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([GetText]() { return GetText(); })
				.Font(Font(TEXT("Bold"), 16, 1))
				.ColorAndOpacity(Colors::GoldHi)
			]
			+ SHorizontalBox::Slot().AutoWidth()[ArrowButton(TEXT(">"), OnNext)];
	}

	/** Slider with a gold fill; Get/Set work in the Min..Max range */
	inline TSharedRef<SWidget> Slider(TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, TFunction<FText(float)> Format)
	{
		static FSliderStyle Style = FSliderStyle()
			.SetNormalBarImage(FSlateNoResource())
			.SetHoveredBarImage(FSlateNoResource())
			.SetDisabledBarImage(FSlateNoResource())
			.SetNormalThumbImage(*Rounded(Colors::GoldHi, 9.f, Hex(TEXT("#0f1218")), 2.f))
			.SetHoveredThumbImage(*Rounded(FLinearColor::White, 9.f, Colors::Gold, 2.f))
			.SetDisabledThumbImage(*Rounded(Colors::Muted, 9.f))
			.SetBarThickness(6.f);
		static FProgressBarStyle Bar = FProgressBarStyle()
			.SetBackgroundImage(*Rounded(Hex(TEXT("#252b38")), 3.f))
			.SetFillImage(*Rounded(Colors::Gold, 3.f))
			.SetMarqueeImage(*Solid(Colors::Gold))
			.SetEnableFillAnimation(false);

		auto Norm = [Get, Min, Max]() { return FMath::GetRangePct(Min, Max, Get()); };

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SBox).HeightOverride(24.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot().VAlign(VAlign_Center).Padding(FMargin(9.f, 0.f))
					[
						SNew(SBox).HeightOverride(6.f)
						[
							SNew(SProgressBar).Style(&Bar).BorderPadding(FVector2D::ZeroVector).Percent_Lambda([Norm]() { return Norm(); })
						]
					]
					+ SOverlay::Slot().VAlign(VAlign_Center)
					[
						SNew(SSlider)
						.Style(&Style)
						.Value_Lambda([Norm]() { return Norm(); })
						.OnValueChanged_Lambda([Set, Min, Max](float V) { Set(FMath::Lerp(Min, Max, V)); })
						.OnMouseCaptureEnd_Lambda([]() { if (UISound) { UISound(true); } })
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
			[
				SNew(SBox).WidthOverride(56.f)
				[
					SNew(STextBlock)
					.Text_Lambda([Get, Format]() { return Format(Get()); })
					.Font(Font(TEXT("Bold"), 15))
					.ColorAndOpacity(Colors::GoldHi)
					.Justification(ETextJustify::Right)
				]
			];
	}

	inline TSharedRef<SWidget> Divider(float Height = 1.f, const FLinearColor& Color = Colors::Line)
	{
		return SNew(SBox).HeightOverride(Height)[SNew(SBorder).BorderImage(Solid(Color))];
	}
}
