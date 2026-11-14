#include "MuseumUIWidget.h"

#include "MuseumSaveGame.h"
#include "MuseumStyle.h"
#include "MuseumUserSettings.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

using namespace MuseumUI;

namespace
{
	/** Fades a flat colour out over the given fraction of the width / height: a gradient without a texture */
	TSharedRef<SWidget> Gradient(bool bHorizontal, const FLinearColor& From, const FLinearColor& To, float Extent = 1.f, int32 Steps = 32)
	{
		auto StripColor = [&](int32 i) { return FMath::Lerp(From, To, float(i) / float(Steps - 1)); };

		if (bHorizontal)
		{
			TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
			for (int32 i = 0; i < Steps; ++i)
			{
				Box->AddSlot().FillWidth(Extent / Steps)[SNew(SBorder).BorderImage(Solid(FLinearColor::White)).BorderBackgroundColor(StripColor(i)).Padding(0.f)];
			}
			if (Extent < 1.f) { Box->AddSlot().FillWidth(1.f - Extent)[SNew(SSpacer)]; }
			return Box;
		}

		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
		for (int32 i = 0; i < Steps; ++i)
		{
			Box->AddSlot().FillHeight(Extent / Steps)[SNew(SBorder).BorderImage(Solid(FLinearColor::White)).BorderBackgroundColor(StripColor(i)).Padding(0.f)];
		}
		if (Extent < 1.f) { Box->AddSlot().FillHeight(1.f - Extent)[SNew(SSpacer)]; }
		return Box;
	}

	TSharedRef<SWidget> Panel(TSharedRef<SWidget> Content, float Pad = 28.f)
	{
		return SNew(SBorder)
			.BorderImage(Rounded(Colors::Panel, 8.f, Colors::Line, 1.f))
			.Padding(Pad)
			[Content];
	}

	FText Num(int32 Value) { return FText::AsNumber(Value); }

	FString FormatTime(float Seconds)
	{
		const int32 Total = FMath::FloorToInt(Seconds);
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}

	/** A key cap and what it does */
	TSharedRef<SWidget> KeyRow(const TCHAR* Key, const TCHAR* Action)
	{
		return SNew(SBox)
			.HeightOverride(34.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).MinDesiredWidth(112.f)
					[
						SNew(SBorder)
						.BorderImage(Rounded(Colors::PanelHi, 4.f, Colors::Line, 1.f))
						.Padding(FMargin(10.f, 3.f))
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock).Text(FText::FromString(Key)).Font(Font(TEXT("Bold"), 14)).ColorAndOpacity(Colors::GoldHi)
						]
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(16.f, 0.f, 0.f, 0.f))
				[
					Label(FText::FromString(Action), 16, Colors::Text)
				]
			];
	}

	/** Big number with a small caption under it */
	TSharedRef<SWidget> Stat(const TCHAR* Caption, TFunction<FText()> Value, int32 Size = 34, const FLinearColor& Color = Colors::Text)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text_Lambda([Value]() { return Value(); }).Font(Font(TEXT("Bold"), Size)).ColorAndOpacity(Color)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Label(FText::FromString(Caption), 12, Colors::Muted, TEXT("Bold"), 3)
			];
	}

	const FButtonStyle& CardStyle()
	{
		static FButtonStyle Style = FButtonStyle()
			.SetNormal(*Rounded(Colors::Panel, 8.f, Colors::Line, 1.5f))
			.SetHovered(*Rounded(Colors::PanelHi, 8.f, Colors::Gold, 3.f))
			.SetPressed(*Rounded(Hex(TEXT("#262d3f")), 8.f, Colors::GoldHi, 3.f))
			.SetNormalForeground(Colors::Text)
			.SetHoveredForeground(Colors::Text)
			.SetPressedForeground(Colors::Text)
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		return Style;
	}

	const FProgressBarStyle& BarStyle(const FLinearColor& Fill)
	{
		// One style per fill colour, kept for the whole run
		static TMap<uint32, TUniquePtr<FProgressBarStyle>> Cache;
		TUniquePtr<FProgressBarStyle>& Entry = Cache.FindOrAdd(Fill.ToFColor(true).DWColor());
		if (!Entry)
		{
			Entry = MakeUnique<FProgressBarStyle>();
			Entry->SetBackgroundImage(*Rounded(FLinearColor(0.f, 0.f, 0.f, 0.55f), 2.f))
				.SetFillImage(*Rounded(Fill, 2.f))
				.SetMarqueeImage(*Solid(Fill))
				.SetEnableFillAnimation(false);
		}
		return *Entry;
	}
}

// ---------------------------------------------------------------------------------------------------------------

void UMuseumUIWidget::Setup(UMuseumDirector* InDirector)
{
	Director = InDirector;
	SetIsFocusable(true);
}

void UMuseumUIWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PageSwitcher.Reset();
	PageLayer.Reset();
	HUDLayer.Reset();
}

TSharedRef<SWidget> UMuseumUIWidget::RebuildWidget()
{
	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(HUDLayer, SBox).Visibility(EVisibility::Collapsed)[BuildHUD()]
		]
		+ SOverlay::Slot()
		[
			SAssignNew(PageLayer, SBox).Visibility(EVisibility::Collapsed)
			[
				SAssignNew(PageSwitcher, SWidgetSwitcher)
				+ SWidgetSwitcher::Slot()[BuildMainPage()]
				+ SWidgetSwitcher::Slot()[BuildSettingsPage()]
				+ SWidgetSwitcher::Slot()[BuildHowToPage()]
				+ SWidgetSwitcher::Slot()[BuildPausePage()]
				+ SWidgetSwitcher::Slot()[BuildPerksPage()]
				+ SWidgetSwitcher::Slot()[BuildEndPage(false)]
				+ SWidgetSwitcher::Slot()[BuildEndPage(true)]
			]
		];
}

void UMuseumUIWidget::ShowPage(EMuseumPage Page)
{
	if (Page == CurrentPage) { return; }
	CurrentPage = Page;
	if (Page == EMuseumPage::Settings) { SettingsTab = 0; }

	if (PageSwitcher.IsValid() && Page != EMuseumPage::None)
	{
		int32 Index = 0;
		switch (Page)
		{
		case EMuseumPage::Main: Index = 0; break;
		case EMuseumPage::Settings: Index = 1; break;
		case EMuseumPage::HowTo: Index = 2; break;
		case EMuseumPage::Pause: Index = 3; break;
		case EMuseumPage::Perks: Index = 4; break;
		case EMuseumPage::GameOver: Index = 5; break;
		case EMuseumPage::Victory: Index = 6; break;
		default: break;
		}
		PageSwitcher->SetActiveWidgetIndex(Index);
		// A quick fade-in on every page change
		PageAlpha = FMath::Min(PageAlpha, 0.2f);
	}
}

void UMuseumUIWidget::TickUI(float RealDelta)
{
	PageAlpha = FMath::FInterpConstantTo(PageAlpha, CurrentPage == EMuseumPage::None ? 0.f : 1.f, RealDelta, 5.f);
	if (PageLayer.IsValid())
	{
		PageLayer->SetVisibility(PageAlpha > 0.01f ? EVisibility::Visible : EVisibility::Collapsed);
		PageLayer->SetRenderOpacity(PageAlpha);
		PageLayer->SetRenderTransform(TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2D(0.f, (1.f - PageAlpha) * 16.f))));
	}

	const UMuseumDirector* D = Dir();
	const EMuseumFlow Flow = D ? D->GetFlow() : EMuseumFlow::Menu;
	const bool bShowHUD = Flow == EMuseumFlow::Playing || Flow == EMuseumFlow::Intermission || Flow == EMuseumFlow::Paused;
	HUDAlpha = FMath::FInterpConstantTo(HUDAlpha, bShowHUD ? 1.f : 0.f, RealDelta, 4.f);
	if (HUDLayer.IsValid())
	{
		HUDLayer->SetVisibility(HUDAlpha > 0.01f ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
		HUDLayer->SetRenderOpacity(HUDAlpha);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UMuseumUIWidget::BuildHUD()
{
	TWeakObjectPtr<UMuseumDirector> W = Director;

	// --- wave header ---
	auto WaveText = [W]()
	{
		const UMuseumDirector* D = W.Get();
		return D ? FText::FromString(FString::Printf(TEXT("WAVE %d / %d"), D->GetWave(), D->GetTotalWaves())) : FText::GetEmpty();
	};
	auto WaveSub = [W]()
	{
		const UMuseumDirector* D = W.Get();
		if (!D) { return FText::GetEmpty(); }
		if (D->GetFlow() == EMuseumFlow::Intermission)
		{
			if (D->GetIntermissionTime() > 0.f)
			{
				return FText::FromString(FString::Printf(TEXT("NEXT WAVE IN %d"), FMath::CeilToInt(D->GetIntermissionTime())));
			}
			return FText::FromString(TEXT("WAVE CLEARED"));
		}
		return FText::FromString(FString::Printf(TEXT("%d ENEMIES LEFT"), D->GetEnemiesLeft()));
	};
	auto WaveFraction = [W]()
	{
		const UMuseumDirector* D = W.Get();
		return (D && D->GetWaveEnemyTotal() > 0) ? float(D->GetEnemiesLeft()) / D->GetWaveEnemyTotal() : 0.f;
	};

	TSharedRef<SWidget> WaveHeader = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text_Lambda(WaveText).Font(Font(TEXT("Bold"), 28, 5)).ColorAndOpacity(Colors::GoldHi)
				.ShadowOffset(FVector2D(1.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 2.f, 0.f, 6.f))
		[
			SNew(STextBlock).Text_Lambda(WaveSub).Font(Font(TEXT("Bold"), 14, 3)).ColorAndOpacity(Colors::Text)
				.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(280.f).HeightOverride(5.f)
			[
				SNew(SProgressBar).Style(&BarStyle(Colors::Gold)).BorderPadding(FVector2D::ZeroVector).Percent_Lambda(WaveFraction)
			]
		];

	// --- boss bar ---
	TSharedRef<SWidget> BossBar = SNew(SBox)
		.WidthOverride(560.f)
		.Visibility_Lambda([W]() { const UMuseumDirector* D = W.Get(); return (D && D->GetBoss() && D->GetFlow() == EMuseumFlow::Playing) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("THE CURATOR"))).Font(Font(TEXT("Bold"), 20, 6)).ColorAndOpacity(Colors::Red)
					.ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
			[
				SNew(SBox).HeightOverride(14.f)
				[
					SNew(SProgressBar).Style(&BarStyle(Colors::Red)).BorderPadding(FVector2D::ZeroVector).Percent_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? D->GetBossHealth() : 0.f; })
				]
			]
		];

	// --- score and combo ---
	TSharedRef<SWidget> ScoreBlock = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			Label(FText::FromString(TEXT("SCORE")), 12, Colors::Muted, TEXT("Bold"), 4)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(STextBlock).Text_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? Num(D->GetScore()) : FText::GetEmpty(); })
				.Font(Font(TEXT("Bold"), 40, 1)).ColorAndOpacity(Colors::Text)
				.ShadowOffset(FVector2D(1.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
		[
			SNew(STextBlock).Text_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? FText::FromString(TEXT("BEST ") + Num(D->GetBestScore()).ToString()) : FText::GetEmpty(); })
				.Font(Font(TEXT("Bold"), 13, 2)).ColorAndOpacity(Colors::Muted)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 12.f, 0.f, 0.f))
		[
			SNew(SBox)
			.Visibility_Lambda([W]() { const UMuseumDirector* D = W.Get(); return (D && D->GetCombo() > 0) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
				[
					SNew(STextBlock).Text_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? FText::FromString(FString::Printf(TEXT("COMBO  x%d"), D->GetMultiplier())) : FText::GetEmpty(); })
						.Font(Font(TEXT("Bold"), 28, 3)).ColorAndOpacity(Colors::Gold)
						.ShadowOffset(FVector2D(1.f, 2.f)).ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 3.f, 0.f, 0.f))
				[
					SNew(SBox).WidthOverride(180.f).HeightOverride(4.f)
					[
						SNew(SProgressBar).Style(&BarStyle(Colors::Gold)).BorderPadding(FVector2D::ZeroVector).Percent_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? D->GetComboFraction() : 0.f; })
					]
				]
			]
		];

	// --- banner ---
	auto BannerAlpha = [W]()
	{
		const UMuseumDirector* D = W.Get();
		if (!D) { return 0.f; }
		const float Age = D->GetBannerAge();
		const float Duration = D->GetBannerDuration();
		if (Age < 0.f || Age > Duration) { return 0.f; }
		return FMath::Min(Age / 0.25f, 1.f) * FMath::Clamp((Duration - Age) / 0.6f, 0.f, 1.f);
	};
	TSharedRef<STextBlock> BannerTitle = SNew(STextBlock)
		.Text_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? D->GetBannerTitle() : FText::GetEmpty(); })
		.Font(Font(TEXT("Bold"), 60, 8))
		.ColorAndOpacity_Lambda([W, BannerAlpha]() { const UMuseumDirector* D = W.Get(); FLinearColor C = D ? D->GetBannerColor() : FLinearColor::White; C.A = BannerAlpha(); return FSlateColor(C); })
		.ShadowOffset(FVector2D(2.f, 3.f))
		.ShadowColorAndOpacity_Lambda([BannerAlpha]() { return FLinearColor(0.f, 0.f, 0.f, 0.85f * BannerAlpha()); })
		.Justification(ETextJustify::Center);
	BannerTitle->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	BannerTitle->SetRenderTransform(TAttribute<TOptional<FSlateRenderTransform>>::CreateLambda([W]() -> TOptional<FSlateRenderTransform>
	{
		// A quick "pop": starts a bit large and settles
		const UMuseumDirector* D = W.Get();
		const float Age = D ? D->GetBannerAge() : 10.f;
		return FSlateRenderTransform(1.f + 0.22f * FMath::Square(FMath::Clamp(1.f - Age / 0.3f, 0.f, 1.f)));
	}));

	TSharedRef<SWidget> Banner = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[BannerTitle]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? D->GetBannerSub() : FText::GetEmpty(); })
			.Font(Font(TEXT("Bold"), 20, 2))
			.ColorAndOpacity_Lambda([BannerAlpha]() { return FSlateColor(FLinearColor(0.93f, 0.91f, 0.87f, BannerAlpha())); })
			.ShadowOffset(FVector2D(1.f, 2.f))
			.ShadowColorAndOpacity_Lambda([BannerAlpha]() { return FLinearColor(0.f, 0.f, 0.f, 0.8f * BannerAlpha()); })
			.Justification(ETextJustify::Center)
		];

	// --- kill feed next to the crosshair ---
	TSharedRef<SVerticalBox> Feed = SNew(SVerticalBox);
	for (int32 i = 0; i < 5; ++i)
	{
		Feed->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([W, i]() { const UMuseumDirector* D = W.Get(); return (D && D->GetFeed().IsValidIndex(i)) ? D->GetFeed()[i].Text : FText::GetEmpty(); })
			.Font(Font(TEXT("Bold"), 19, 1))
			.ColorAndOpacity_Lambda([W, i]()
			{
				const UMuseumDirector* D = W.Get();
				if (!D || !D->GetFeed().IsValidIndex(i)) { return FSlateColor(FLinearColor::Transparent); }
				FLinearColor C = D->GetFeed()[i].Color;
				C.A = 1.f - FMath::SmoothStep(2.6f, 3.8f, D->GetFeed()[i].Age);
				return FSlateColor(C);
			})
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f))
		];
	}

	// --- perk chips ---
	TSharedRef<SHorizontalBox> Chips = SNew(SHorizontalBox);
	for (int32 i = 0; i < (int32)EMuseumPerk::Count; ++i)
	{
		const EMuseumPerk Perk = (EMuseumPerk)i;
		Chips->AddSlot().AutoWidth().Padding(FMargin(3.f, 0.f))
		[
			SNew(SBox)
			.Visibility_Lambda([W, Perk]() { const UMuseumDirector* D = W.Get(); return (D && D->GetPerkLevel(Perk) > 0) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SBorder)
				.BorderImage(Rounded(FLinearColor(0.f, 0.f, 0.f, 0.55f), 4.f, UMuseumDirector::GetPerkInfo(Perk).Color, 1.5f))
				.Padding(FMargin(8.f, 3.f))
				[
					SNew(STextBlock)
					.Text_Lambda([W, Perk]() { const UMuseumDirector* D = W.Get(); return FText::FromString(FString::Printf(TEXT("%s  x%d"), *UMuseumDirector::GetPerkInfo(Perk).Name.ToString(), D ? D->GetPerkLevel(Perk) : 0)); })
					.Font(Font(TEXT("Bold"), 11, 1))
					.ColorAndOpacity(UMuseumDirector::GetPerkInfo(Perk).Color)
				]
			]
		];
	}

	// --- FPS counter ---
	TSharedRef<float> Fps = MakeShared<float>(60.f);

	return SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 18.f, 0.f, 0.f))[WaveHeader]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f, 124.f, 0.f, 0.f))[BossBar]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f, 16.f, 34.f, 0.f))[ScoreBlock]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 330.f))[Banner]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(380.f, 0.f, 0.f, 40.f))[Feed]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f, 0.f, 0.f, 26.f))[Chips]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(14.f, 10.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Visibility_Lambda([]() { const UMuseumUserSettings* S = UMuseumUserSettings::Get(); return (S && S->bShowFPS) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.Text_Lambda([Fps]()
			{
				*Fps = FMath::Lerp(*Fps, 1.f / FMath::Max(FApp::GetDeltaTime(), 0.0001f), 0.04f);
				return FText::FromString(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(*Fps)));
			})
			.Font(Font(TEXT("Bold"), 13, 1))
			.ColorAndOpacity(Colors::Green)
		];
}

// ---------------------------------------------------------------------------------------------------------------
// Main menu / pause / how to play
// ---------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UMuseumUIWidget::BuildMainPage()
{
	TWeakObjectPtr<UMuseumDirector> W = Director;
	auto Stats = [W]()
	{
		const UMuseumDirector* D = W.Get();
		const UMuseumSaveGame* S = D ? D->GetSave() : nullptr;
		if (!S || S->GamesPlayed == 0) { return FText::FromString(TEXT("Survive eight waves until dawn.")); }
		return FText::FromString(FString::Printf(TEXT("BEST SCORE  %s      BEST WAVE  %d / %d      NIGHTS SURVIVED  %d"),
			*Num(S->BestScore).ToString(), S->BestWave, D->GetTotalWaves(), S->Victories));
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()[Gradient(true, Hex(TEXT("#04060a"), 0.94f), Hex(TEXT("#04060a"), 0.f), 0.66f)]
		+ SOverlay::Slot()[Gradient(false, Hex(TEXT("#04060a"), 0.f), Hex(TEXT("#04060a"), 0.7f), 0.28f)]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(100.f, 0.f, 0.f, 20.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[Label(FText::FromString(TEXT("A  MUSEUM  SURVIVAL  SHOOTER")), 15, Colors::Gold, TEXT("Bold"), 6)]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))[Label(FText::FromString(TEXT("BULLET")), 104, Colors::Text, TEXT("Bold"), 10)]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, -34.f, 0.f, 0.f))[Label(FText::FromString(TEXT("GALLERY")), 104, Colors::Gold, TEXT("Bold"), 10)]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.f, 6.f, 0.f, 34.f))
			[
				Label(FText::FromString(TEXT("The exhibits wake up after dark. Hold the night.")), 20, Colors::Muted, TEXT("Italic"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))[Button(FText::FromString(TEXT("PLAY")), [W]() { if (UMuseumDirector* D = W.Get()) { D->StartGame(); } }, true, 360.f, 58.f, 22)]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Button(FText::FromString(TEXT("SETTINGS")), [W]() { if (UMuseumDirector* D = W.Get()) { D->ShowSettings(); } }, false, 360.f, 50.f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Button(FText::FromString(TEXT("HOW TO PLAY")), [W]() { if (UMuseumDirector* D = W.Get()) { D->ShowHowTo(); } }, false, 360.f, 50.f)]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 30.f))[Button(FText::FromString(TEXT("QUIT")), [W]() { if (UMuseumDirector* D = W.Get()) { D->QuitGame(); } }, false, 360.f, 50.f)]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text_Lambda(Stats).Font(Font(TEXT("Bold"), 13, 2)).ColorAndOpacity(Colors::Muted)
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(100.f, 0.f, 0.f, 26.f))
		[
			Label(FText::FromString(TEXT("v1.0   -   Made with Unreal Engine 5")), 12, Colors::Muted, TEXT("Regular"), 1)
		];
}

TSharedRef<SWidget> UMuseumUIWidget::BuildPausePage()
{
	TWeakObjectPtr<UMuseumDirector> W = Director;
	auto Summary = [W]()
	{
		const UMuseumDirector* D = W.Get();
		return D ? FText::FromString(FString::Printf(TEXT("WAVE %d / %d     SCORE %s     KILLS %d"), D->GetWave(), D->GetTotalWaves(), *Num(D->GetScore()).ToString(), D->GetKills())) : FText::GetEmpty();
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(Solid(Hex(TEXT("#03050a"), 0.62f)))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			Panel(
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(FText::FromString(TEXT("PAUSED")), 48, Colors::Text, TEXT("Bold"), 10)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f, 0.f, 26.f))
				[
					SNew(STextBlock).Text_Lambda(Summary).Font(Font(TEXT("Bold"), 13, 2)).ColorAndOpacity(Colors::Muted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Button(FText::FromString(TEXT("RESUME")), [W]() { if (UMuseumDirector* D = W.Get()) { D->Resume(); } }, true, 380.f, 54.f, 20)]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Button(FText::FromString(TEXT("SETTINGS")), [W]() { if (UMuseumDirector* D = W.Get()) { D->ShowSettings(); } }, false, 380.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Button(FText::FromString(TEXT("HOW TO PLAY")), [W]() { if (UMuseumDirector* D = W.Get()) { D->ShowHowTo(); } }, false, 380.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Button(FText::FromString(TEXT("RESTART THE NIGHT")), [W]() { if (UMuseumDirector* D = W.Get()) { D->RestartRun(); } }, false, 380.f)]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Button(FText::FromString(TEXT("MAIN MENU")), [W]() { if (UMuseumDirector* D = W.Get()) { D->ReturnToMenu(); } }, false, 380.f)]
				+ SVerticalBox::Slot().AutoHeight()[Button(FText::FromString(TEXT("QUIT TO DESKTOP")), [W]() { if (UMuseumDirector* D = W.Get()) { D->QuitGame(); } }, false, 380.f)],
				42.f)
		];
}

TSharedRef<SWidget> UMuseumUIWidget::BuildHowToPage()
{
	TWeakObjectPtr<UMuseumDirector> W = Director;

	auto Bullet = [](const TCHAR* Text)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.f, 3.f, 10.f, 0.f))[Label(FText::FromString(TEXT("-")), 16, Colors::Gold, TEXT("Bold"))]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(TEXT("Regular"), 16)).ColorAndOpacity(Colors::Text).AutoWrapText(true)
			];
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(Solid(Hex(TEXT("#03050a"), 0.7f)))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(1060.f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Label(FText::FromString(TEXT("HOW TO PLAY")), 36, Colors::Text, TEXT("Bold"), 8)]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 14.f))[Divider()]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f, 0.f, 36.f, 0.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Label(FText::FromString(TEXT("THE NIGHT")), 15, Colors::Gold, TEXT("Bold"), 5)]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[Bullet(TEXT("The museum's statues and guards come to life at dusk. Survive all eight waves until dawn."))]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[Bullet(TEXT("Shots to the head deal double damage. Chain kills quickly to build a combo multiplier."))]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[Bullet(TEXT("Enemies drop medkits and ammo. Between waves you get supplies and pick one perk."))]
							+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 8.f))[Bullet(TEXT("Pick up crates and throw them. Use the glass doors and corners to break line of sight."))]
							+ SVerticalBox::Slot().AutoHeight()[Bullet(TEXT("The Curator waits at the end. Good luck."))]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(430.f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 10.f))[Label(FText::FromString(TEXT("CONTROLS")), 15, Colors::Gold, TEXT("Bold"), 5)]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("W A S D"), TEXT("Move"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("MOUSE"), TEXT("Look around"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("LMB"), TEXT("Fire"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("RMB"), TEXT("Aim (zoom, steadier)"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("R"), TEXT("Reload"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("SHIFT"), TEXT("Walk quietly"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("SPACE"), TEXT("Jump"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("E"), TEXT("Pick up / drop (LMB throws)"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("F"), TEXT("Flashlight"))]
								+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("ESC"), TEXT("Pause"))]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 22.f, 0.f, 0.f)).HAlign(HAlign_Right)
					[
						Button(FText::FromString(TEXT("BACK")), [W]() { if (UMuseumDirector* D = W.Get()) { D->OnBackPressed(); } }, true, 200.f, 48.f)
					],
					36.f)
			]
		];
}

// ---------------------------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------------------------

namespace
{
	UMuseumUserSettings* Settings() { return UMuseumUserSettings::Get(); }

	const TCHAR* QualityNames[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };

	FText QualityText(int32 Level)
	{
		return FText::FromString(Level >= 0 && Level < 4 ? QualityNames[Level] : TEXT("CUSTOM"));
	}

	/** A "< LOW >" selector for one of the engine's quality groups */
	TSharedRef<SWidget> QualityRow(const TCHAR* Name, TFunction<int32()> Get, TFunction<void(int32)> Set)
	{
		return Row(FText::FromString(Name), Cycler(
			[Get]() { return QualityText(Get()); },
			[Get, Set]() { Set(FMath::Max(Get() - 1, 0)); },
			[Get, Set]() { Set(FMath::Min(Get() + 1, 3)); }));
	}

	TSharedRef<SWidget> ToggleRow(const TCHAR* Name, TFunction<bool()> Get, TFunction<void(bool)> Set)
	{
		auto Flip = [Get, Set]() { Set(!Get()); };
		return Row(FText::FromString(Name), Cycler([Get]() { return FText::FromString(Get() ? TEXT("ON") : TEXT("OFF")); }, Flip, Flip));
	}

	TSharedRef<SWidget> PercentSlider(TFunction<float()> Get, TFunction<void(float)> Set)
	{
		return Slider(Get, Set, 0.f, 1.f, [](float V) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V * 100.f))); });
	}

	/** The preset every quality group agrees on (0..3), or -1 when they differ ("custom"); the resolution scale is not part of it */
	int32 PresetLevel()
	{
		const UMuseumUserSettings* S = Settings();
		if (!S) { return 2; }
		const int32 Levels[] = { S->GetViewDistanceQuality(), S->GetAntiAliasingQuality(), S->GetShadowQuality(), S->GetGlobalIlluminationQuality(),
			S->GetReflectionQuality(), S->GetPostProcessingQuality(), S->GetTextureQuality(), S->GetVisualEffectQuality(), S->GetFoliageQuality(), S->GetShadingQuality() };
		for (int32 Level : Levels) { if (Level != Levels[0]) { return -1; } }
		return Levels[0];
	}

	/** Re-applies the graphics options that do not change the window */
	void ApplyGraphics()
	{
		if (UMuseumUserSettings* S = Settings()) { S->ApplyNonResolutionSettings(); S->SaveSettings(); }
	}

	void Changed()
	{
		if (UMuseumUserSettings* S = Settings()) { S->OnChanged.Broadcast(); }
	}
}

TSharedRef<SWidget> UMuseumUIWidget::BuildGraphicsTab()
{
	// Resolutions the display supports; the current one is always in the list
	if (Resolutions.Num() == 0)
	{
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		if (Resolutions.Num() == 0) { UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions); }
		if (const UMuseumUserSettings* S = Settings()) { Resolutions.AddUnique(S->GetScreenResolution()); }
		Resolutions.RemoveAll([](const FIntPoint& R) { return R.X < 1024 || R.Y < 576; });
		Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	}
	static const float FrameLimits[] = { 0.f, 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f };
	TArray<FIntPoint>* Res = &Resolutions;

	auto ResIndex = [Res]()
	{
		const UMuseumUserSettings* S = Settings();
		const int32 Found = S ? Res->Find(S->GetScreenResolution()) : INDEX_NONE;
		return Found == INDEX_NONE ? 0 : Found;
	};
	auto SetRes = [Res](int32 Index)
	{
		if (UMuseumUserSettings* S = Settings(); S && Res->Num() > 0)
		{
			S->SetScreenResolution((*Res)[FMath::Clamp(Index, 0, Res->Num() - 1)]);
			S->ApplyResolutionSettings(false);
			S->SaveSettings();
		}
	};
	auto ModeIndex = []()
	{
		const UMuseumUserSettings* S = Settings();
		switch (S ? S->GetFullscreenMode() : EWindowMode::Windowed)
		{
		case EWindowMode::Fullscreen: return 0;
		case EWindowMode::WindowedFullscreen: return 1;
		default: return 2;
		}
	};
	auto SetMode = [](int32 Index)
	{
		if (UMuseumUserSettings* S = Settings())
		{
			const EWindowMode::Type Modes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
			S->SetFullscreenMode(Modes[FMath::Clamp(Index, 0, 2)]);
			S->ApplyResolutionSettings(false);
			S->SaveSettings();
		}
	};
	auto LimitIndex = []()
	{
		const UMuseumUserSettings* S = Settings();
		const float Limit = S ? S->GetFrameRateLimit() : 0.f;
		for (int32 i = 0; i < UE_ARRAY_COUNT(FrameLimits); ++i) { if (FMath::IsNearlyEqual(FrameLimits[i], Limit)) { return i; } }
		return 0;
	};
	auto SetLimit = [](int32 Index)
	{
		if (UMuseumUserSettings* S = Settings()) { S->SetFrameRateLimit(FrameLimits[FMath::Clamp(Index, 0, (int32)UE_ARRAY_COUNT(FrameLimits) - 1)]); ApplyGraphics(); }
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Window mode")), Cycler(
			[ModeIndex]() { const TCHAR* Names[] = { TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED") }; return FText::FromString(Names[ModeIndex()]); },
			[ModeIndex, SetMode]() { SetMode(FMath::Max(ModeIndex() - 1, 0)); },
			[ModeIndex, SetMode]() { SetMode(FMath::Min(ModeIndex() + 1, 2)); }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Resolution")), Cycler(
			[Res, ResIndex]() { return Res->Num() > 0 ? FText::FromString(FString::Printf(TEXT("%d x %d"), (*Res)[ResIndex()].X, (*Res)[ResIndex()].Y)) : FText::GetEmpty(); },
			[ResIndex, SetRes]() { SetRes(ResIndex() - 1); },
			[ResIndex, SetRes]() { SetRes(ResIndex() + 1); }))]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("VSync"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->IsVSyncEnabled(); },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->SetVSyncEnabled(bOn); ApplyGraphics(); } })]
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Frame rate limit")), Cycler(
			[LimitIndex]() { const float L = FrameLimits[LimitIndex()]; return L <= 0.f ? FText::FromString(TEXT("UNLIMITED")) : FText::FromString(FString::Printf(TEXT("%d FPS"), (int32)L)); },
			[LimitIndex, SetLimit]() { SetLimit(LimitIndex() - 1); },
			[LimitIndex, SetLimit]() { SetLimit(LimitIndex() + 1); }))]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f))[Divider()]
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Quality preset")), Cycler(
			[]() { return QualityText(PresetLevel()); },
			[]() { if (UMuseumUserSettings* S = Settings()) { S->SetOverallScalabilityLevel(FMath::Max(PresetLevel() < 0 ? 2 : PresetLevel() - 1, 0)); S->ApplyResolutionScale(); ApplyGraphics(); } },
			[]() { if (UMuseumUserSettings* S = Settings()) { S->SetOverallScalabilityLevel(FMath::Min(PresetLevel() < 0 ? 2 : PresetLevel() + 1, 3)); S->ApplyResolutionScale(); ApplyGraphics(); } }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Resolution scale")), Slider(
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->ResolutionScalePercent : 100.f; },
			[](float V) { if (UMuseumUserSettings* S = Settings()) { S->ResolutionScalePercent = FMath::RoundToFloat(V / 5.f) * 5.f; S->ApplyResolutionScale(); ApplyGraphics(); } },
			50.f, 100.f, [](float V) { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(V))); }))]
		+ SVerticalBox::Slot().AutoHeight()[QualityRow(TEXT("Shadows"),
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->GetShadowQuality() : 2; },
			[](int32 V) { if (UMuseumUserSettings* S = Settings()) { S->SetShadowQuality(V); ApplyGraphics(); } })]
		+ SVerticalBox::Slot().AutoHeight()[QualityRow(TEXT("Global illumination"),
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->GetGlobalIlluminationQuality() : 2; },
			[](int32 V) { if (UMuseumUserSettings* S = Settings()) { S->SetGlobalIlluminationQuality(V); ApplyGraphics(); } })]
		+ SVerticalBox::Slot().AutoHeight()[QualityRow(TEXT("Effects"),
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->GetVisualEffectQuality() : 2; },
			[](int32 V) { if (UMuseumUserSettings* S = Settings()) { S->SetVisualEffectQuality(V); ApplyGraphics(); } })]
		+ SVerticalBox::Slot().AutoHeight()[QualityRow(TEXT("Textures"),
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->GetTextureQuality() : 2; },
			[](int32 V) { if (UMuseumUserSettings* S = Settings()) { S->SetTextureQuality(V); ApplyGraphics(); } })]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f))[Divider()]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("Motion blur"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->bMotionBlur; },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->bMotionBlur = bOn; S->SaveSettings(); Changed(); } })]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("Film grain"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->bFilmGrain; },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->bFilmGrain = bOn; S->SaveSettings(); Changed(); } })];
}

TSharedRef<SWidget> UMuseumUIWidget::BuildAudioTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Master volume")), PercentSlider(
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->MasterVolume : 1.f; },
			[](float V) { if (UMuseumUserSettings* S = Settings()) { S->MasterVolume = V; Changed(); } }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Music")), PercentSlider(
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->MusicVolume : 1.f; },
			[](float V) { if (UMuseumUserSettings* S = Settings()) { S->MusicVolume = V; Changed(); } }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Effects")), PercentSlider(
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->SfxVolume : 1.f; },
			[](float V) { if (UMuseumUserSettings* S = Settings()) { S->SfxVolume = V; Changed(); } }))];
}

TSharedRef<SWidget> UMuseumUIWidget::BuildGameplayTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Mouse sensitivity")), Slider(
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->MouseSensitivity : 1.f; },
			[](float V) { if (UMuseumUserSettings* S = Settings()) { S->MouseSensitivity = FMath::RoundToFloat(V * 20.f) / 20.f; } },
			0.2f, 3.f, [](float V) { return FText::FromString(FString::Printf(TEXT("%.2fx"), V)); }))]
		+ SVerticalBox::Slot().AutoHeight()[Row(FText::FromString(TEXT("Field of view")), Slider(
			[]() { const UMuseumUserSettings* S = Settings(); return S ? S->FieldOfView : 90.f; },
			[](float V) { if (UMuseumUserSettings* S = Settings()) { S->FieldOfView = FMath::RoundToFloat(V); } },
			70.f, 110.f, [](float V) { return FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(V))); }))]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("Invert Y axis"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->bInvertY; },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->bInvertY = bOn; } })]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("Camera shake"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->bCameraShake; },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->bCameraShake = bOn; } })]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("Head bob"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->bHeadBob; },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->bHeadBob = bOn; } })]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("Damage numbers"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->bDamageNumbers; },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->bDamageNumbers = bOn; } })]
		+ SVerticalBox::Slot().AutoHeight()[ToggleRow(TEXT("Show FPS"),
			[]() { const UMuseumUserSettings* S = Settings(); return S && S->bShowFPS; },
			[](bool bOn) { if (UMuseumUserSettings* S = Settings()) { S->bShowFPS = bOn; } })];
}

TSharedRef<SWidget> UMuseumUIWidget::BuildControlsTab()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 12.f))
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Default controls (keyboard and mouse). A gamepad also works for movement and aiming."))).Font(Font(TEXT("Regular"), 15)).ColorAndOpacity(Colors::Muted)
		]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("W A S D"), TEXT("Move"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("MOUSE"), TEXT("Look around"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("LMB"), TEXT("Fire"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("RMB"), TEXT("Aim"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("R"), TEXT("Reload"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("SHIFT"), TEXT("Walk quietly"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("SPACE"), TEXT("Jump"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("E"), TEXT("Pick up / drop an object, LMB to throw"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("F"), TEXT("Flashlight on / off"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("1  2  3"), TEXT("Choose a perk between waves"))]
		+ SVerticalBox::Slot().AutoHeight()[KeyRow(TEXT("ESC"), TEXT("Pause / back"))];
}

TSharedRef<SWidget> UMuseumUIWidget::BuildSettingsPage()
{
	TWeakObjectPtr<UMuseumDirector> W = Director;
	TWeakObjectPtr<UMuseumUIWidget> Self = this;

	// Tab buttons: a plain and a highlighted copy, the right one is shown
	auto Tab = [Self](const TCHAR* Name, int32 Index)
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBox).Visibility_Lambda([Self, Index]() { return (Self.IsValid() && Self->SettingsTab == Index) ? EVisibility::Collapsed : EVisibility::Visible; })
				[Button(FText::FromString(Name), [Self, Index]() { if (Self.IsValid()) { Self->SettingsTab = Index; } }, false, 190.f, 42.f, 15)]
			]
			+ SOverlay::Slot()
			[
				SNew(SBox).Visibility_Lambda([Self, Index]() { return (Self.IsValid() && Self->SettingsTab == Index) ? EVisibility::Visible : EVisibility::Collapsed; })
				[Button(FText::FromString(Name), []() {}, true, 190.f, 42.f, 15)]
			];
	};

	auto ResetTab = [Self]()
	{
		UMuseumUserSettings* S = Settings();
		if (!S || !Self.IsValid()) { return; }
		switch (Self->SettingsTab)
		{
		case 0: S->ResetGraphics(); S->ApplyNonResolutionSettings(); break;
		case 1: S->ResetAudio(); break;
		case 2: S->ResetGameplay(); break;
		default: break;
		}
		S->SaveSettings();
		S->OnChanged.Broadcast();
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(Solid(Hex(TEXT("#03050a"), 0.72f)))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(940.f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[Label(FText::FromString(TEXT("SETTINGS")), 36, Colors::Text, TEXT("Bold"), 8)]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 16.f, 0.f, 16.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))[Tab(TEXT("GRAPHICS"), 0)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))[Tab(TEXT("AUDIO"), 1)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 10.f, 0.f))[Tab(TEXT("GAMEPLAY"), 2)]
						+ SHorizontalBox::Slot().AutoWidth()[Tab(TEXT("CONTROLS"), 3)]
					]
					+ SVerticalBox::Slot().AutoHeight()[Divider()]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 0.f))
					[
						SNew(SBox).HeightOverride(600.f)
						[
							SNew(SWidgetSwitcher)
							.WidgetIndex_Lambda([Self]() { return Self.IsValid() ? Self->SettingsTab : 0; })
							+ SWidgetSwitcher::Slot()[BuildGraphicsTab()]
							+ SWidgetSwitcher::Slot()[BuildAudioTab()]
							+ SWidgetSwitcher::Slot()[BuildGameplayTab()]
							+ SWidgetSwitcher::Slot()[BuildControlsTab()]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).Visibility_Lambda([Self]() { return (Self.IsValid() && Self->SettingsTab < 3) ? EVisibility::Visible : EVisibility::Hidden; })
							[Button(FText::FromString(TEXT("RESET THIS TAB")), ResetTab, false, 230.f, 46.f, 14)]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f)[SNew(SSpacer)]
						+ SHorizontalBox::Slot().AutoWidth()
						[Button(FText::FromString(TEXT("BACK")), [W]() { if (UMuseumDirector* D = W.Get()) { D->CloseSettings(); } }, true, 200.f, 46.f)]
					],
					34.f)
			]
		];
}

// ---------------------------------------------------------------------------------------------------------------
// Perks, game over, victory
// ---------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UMuseumUIWidget::BuildPerksPage()
{
	TWeakObjectPtr<UMuseumDirector> W = Director;

	auto OfferInfo = [W](int32 Index, TFunction<FText(const FPerkInfo&, int32)> Pick)
	{
		const UMuseumDirector* D = W.Get();
		if (!D || !D->GetOffers().IsValidIndex(Index)) { return FText::GetEmpty(); }
		const EMuseumPerk Perk = D->GetOffers()[Index];
		return Pick(UMuseumDirector::GetPerkInfo(Perk), D->GetPerkLevel(Perk));
	};

	auto Card = [W, OfferInfo](int32 Index)
	{
		return SNew(SBox)
			.WidthOverride(320.f)
			.HeightOverride(340.f)
			[
				SNew(SButton)
				.ButtonStyle(&CardStyle())
				.OnHovered_Lambda([]() { if (UISound) { UISound(false); } })
				.OnClicked_Lambda([W, Index]()
				{
					if (UISound) { UISound(true); }
					if (UMuseumDirector* D = W.Get()) { D->ChoosePerk(Index); }
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(8.f)
						[
							SNew(SBorder).BorderImage(Solid(FLinearColor::White))
							.BorderBackgroundColor_Lambda([W, Index]()
							{
								const UMuseumDirector* D = W.Get();
								return (D && D->GetOffers().IsValidIndex(Index)) ? FSlateColor(UMuseumDirector::GetPerkInfo(D->GetOffers()[Index]).Color) : FSlateColor(Colors::Gold);
							})
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(26.f, 20.f, 26.f, 0.f))
					[
						Label(FText::FromString(FString::Printf(TEXT("[ %d ]"), Index + 1)), 15, Colors::Muted, TEXT("Bold"), 3)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(26.f, 14.f, 26.f, 0.f))
					[
						SNew(STextBlock).AutoWrapText(true).Font(Font(TEXT("Bold"), 28, 2)).ColorAndOpacity(Colors::GoldHi)
							.Text_Lambda([OfferInfo, Index]() { return OfferInfo(Index, [](const FPerkInfo& Info, int32) { return Info.Name; }); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(26.f, 8.f, 26.f, 0.f))
					[
						SNew(STextBlock).Font(Font(TEXT("Bold"), 13, 3)).ColorAndOpacity(Colors::Muted)
							.Text_Lambda([OfferInfo, Index]() { return OfferInfo(Index, [](const FPerkInfo&, int32 Level) { return FText::FromString(Level == 0 ? TEXT("NEW") : FString::Printf(TEXT("LEVEL %d  >  %d"), Level, Level + 1)); }); })
					]
					+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(26.f, 22.f, 26.f, 24.f))
					[
						SNew(STextBlock).AutoWrapText(true).Font(Font(TEXT("Regular"), 19)).ColorAndOpacity(Colors::Text)
							.Text_Lambda([OfferInfo, Index]() { return OfferInfo(Index, [](const FPerkInfo& Info, int32) { return Info.Description; }); })
					]
				]
			];
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(Solid(Hex(TEXT("#03050a"), 0.78f)))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(Font(TEXT("Bold"), 20, 6)).ColorAndOpacity(Colors::Green)
					.Text_Lambda([W]() { const UMuseumDirector* D = W.Get(); return D ? FText::FromString(FString::Printf(TEXT("WAVE %d CLEARED"), D->GetWave())) : FText::GetEmpty(); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f, 0.f, 4.f))
			[
				Label(FText::FromString(TEXT("CHOOSE A PERK")), 54, Colors::Text, TEXT("Bold"), 8)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 32.f))
			[
				Label(FText::FromString(TEXT("Click a card or press 1, 2 or 3")), 16, Colors::Muted)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(14.f, 0.f))[Card(0)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(14.f, 0.f))[Card(1)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(14.f, 0.f))[Card(2)]
			]
		];
}

TSharedRef<SWidget> UMuseumUIWidget::BuildEndPage(bool bVictory)
{
	TWeakObjectPtr<UMuseumDirector> W = Director;
	const FLinearColor Accent = bVictory ? Colors::GoldHi : Colors::Red;

	auto Stat2 = [W](const TCHAR* Caption, TFunction<FText(const UMuseumDirector&)> Get, int32 Size = 34, const FLinearColor& Color = Colors::Text)
	{
		return Stat(Caption, [W, Get]() { const UMuseumDirector* D = W.Get(); return D ? Get(*D) : FText::GetEmpty(); }, Size, Color);
	};

	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBorder).BorderImage(Solid(bVictory ? Hex(TEXT("#0a0804"), 0.7f) : Hex(TEXT("#0a0304"), 0.78f)))]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(820.f)
			[
				Panel(
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						Label(FText::FromString(bVictory ? TEXT("DAWN  BREAKS") : TEXT("THE  MUSEUM  WINS")), 56, Accent, TEXT("Bold"), 8)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f, 0.f, 0.f))
					[
						Label(FText::FromString(bVictory ? TEXT("You held the night. The exhibits are quiet again.") : TEXT("The exhibits are quiet again. Until next time.")), 18, Colors::Muted, TEXT("Italic"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 24.f))[Divider()]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[Stat2(TEXT("SCORE"), [](const UMuseumDirector& D) { return Num(D.GetScore()); }, 46, Colors::GoldHi)]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[Stat2(TEXT("WAVE"), [](const UMuseumDirector& D) { return FText::FromString(FString::Printf(TEXT("%d / %d"), D.GetWave(), D.GetTotalWaves())); })]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[Stat2(TEXT("TIME"), [](const UMuseumDirector& D) { return FText::FromString(FormatTime(D.GetRunTime())); })]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[Stat2(TEXT("KILLS"), [](const UMuseumDirector& D) { return Num(D.GetKills()); }, 28)]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)[Stat2(TEXT("HEADSHOTS"), [](const UMuseumDirector& D) { return Num(D.GetHeadshots()); }, 28)]
						+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)
						[
							SNew(SBox)
							.Visibility(bVictory ? EVisibility::Visible : EVisibility::Collapsed)
							[Stat2(TEXT("RANK"), [](const UMuseumDirector& D) { return FText::FromString(D.GetRank()); }, 28, Colors::GoldHi)]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 22.f, 0.f, 0.f))
					[
						SNew(STextBlock)
						.Text_Lambda([W]() { const UMuseumDirector* D = W.Get(); return (D && D->IsNewBest()) ? FText::FromString(TEXT("NEW PERSONAL BEST")) : FText::FromString(D ? FString::Printf(TEXT("PERSONAL BEST  %s"), *Num(D->GetBestScore()).ToString()) : FString()); })
						.ColorAndOpacity_Lambda([W]() { const UMuseumDirector* D = W.Get(); return FSlateColor((D && D->IsNewBest()) ? Colors::Green : Colors::Muted); })
						.Font(Font(TEXT("Bold"), 16, 4))
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 30.f, 0.f, 0.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 14.f, 0.f))[Button(FText::FromString(bVictory ? TEXT("PLAY AGAIN") : TEXT("TRY AGAIN")), [W]() { if (UMuseumDirector* D = W.Get()) { D->RestartRun(); } }, true, 280.f, 54.f, 19)]
						+ SHorizontalBox::Slot().AutoWidth()[Button(FText::FromString(TEXT("MAIN MENU")), [W]() { if (UMuseumDirector* D = W.Get()) { D->ReturnToMenu(); } }, false, 280.f, 54.f, 19)]
					],
					40.f)
			]
		];
}
