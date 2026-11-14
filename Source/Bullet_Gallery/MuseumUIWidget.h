#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MuseumDirector.h"
#include "MuseumUIWidget.generated.h"

class SBox;
class SWidgetSwitcher;
class STextBlock;

/**
 * Everything the game director shows on the screen, built entirely with Slate code (no Blueprint, no textures):
 * the main menu, the pause menu, the settings, the how-to-play page, the perk cards, the game over and victory screens,
 * and the HUD parts of the wave game (wave counter, score, combo, banners, boss bar, kill feed, perk chips).
 * The health, ammo and message parts of the HUD stay in UMuseumHUDWidget.
 */
UCLASS()
class BULLET_GALLERY_API UMuseumUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UMuseumDirector* InDirector);

	/** Opens a screen (None closes the menu layer and shows only the HUD) */
	void ShowPage(EMuseumPage Page);
	EMuseumPage GetPage() const { return CurrentPage; }

	/** Driven by the director every frame, in real time, so it also runs while the game is paused */
	void TickUI(float RealDelta);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedRef<SWidget> BuildHUD();
	TSharedRef<SWidget> BuildMainPage();
	TSharedRef<SWidget> BuildPausePage();
	TSharedRef<SWidget> BuildSettingsPage();
	TSharedRef<SWidget> BuildHowToPage();
	TSharedRef<SWidget> BuildPerksPage();
	TSharedRef<SWidget> BuildEndPage(bool bVictory);

	TSharedRef<SWidget> BuildGraphicsTab();
	TSharedRef<SWidget> BuildAudioTab();
	TSharedRef<SWidget> BuildGameplayTab();
	TSharedRef<SWidget> BuildControlsTab();

	UMuseumDirector* Dir() const { return Director.Get(); }

	TWeakObjectPtr<UMuseumDirector> Director;
	EMuseumPage CurrentPage = EMuseumPage::None;
	int32 SettingsTab = 0;

	TSharedPtr<SWidgetSwitcher> PageSwitcher;
	TSharedPtr<SBox> PageLayer;
	TSharedPtr<SBox> HUDLayer;

	/** 0..1 fade of the page layer, and of the HUD */
	float PageAlpha = 0.f;
	float HUDAlpha = 0.f;

	TArray<FIntPoint> Resolutions;
};
