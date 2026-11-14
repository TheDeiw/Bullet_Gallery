#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "MuseumUserSettings.generated.h"

/**
 * Player settings saved in GameUserSettings.ini: the engine's own graphics options (resolution, window mode, vsync,
 * quality levels) plus the game's own audio, camera and gameplay options. Registered through
 * GameUserSettingsClassName in DefaultEngine.ini.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults)
class BULLET_GALLERY_API UMuseumUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UMuseumUserSettings();

	/** The settings object of the running game (never null once the engine is up) */
	static UMuseumUserSettings* Get();

	virtual void SetToDefaults() override;

	/** Restores only the options of the active tab (graphics / audio / gameplay) to their defaults */
	void ResetGraphics();
	void ResetAudio();
	void ResetGameplay();

	/** Pushes ResolutionScalePercent to the engine (r.ScreenPercentage; the scalability group value cannot be read back reliably) */
	void ApplyResolutionScale();

	/** Applies the graphics options right now and writes everything to disk */
	void ApplyAndSave();

	// ---------- Audio (0..1) ----------
	UPROPERTY(Config) float MasterVolume = 0.8f;
	UPROPERTY(Config) float MusicVolume = 0.6f;
	UPROPERTY(Config) float SfxVolume = 0.9f;

	// ---------- Gameplay ----------
	/** Multiplier of the mouse look speed */
	UPROPERTY(Config) float MouseSensitivity = 1.f;
	UPROPERTY(Config) float FieldOfView = 90.f;
	UPROPERTY(Config) bool bInvertY = false;
	UPROPERTY(Config) bool bCameraShake = true;
	UPROPERTY(Config) bool bHeadBob = true;
	UPROPERTY(Config) bool bDamageNumbers = true;
	UPROPERTY(Config) bool bShowFPS = false;

	// ---------- Graphics extras ----------
	/** Percent of the screen resolution that is rendered (the engine's own value is not reliable to read back) */
	UPROPERTY(Config) float ResolutionScalePercent = 100.f;
	UPROPERTY(Config) bool bMotionBlur = false;
	UPROPERTY(Config) bool bFilmGrain = true;

	/** Fired after any option changed so that live objects (audio mix, post process) can refresh */
	DECLARE_MULTICAST_DELEGATE(FOnChanged);
	FOnChanged OnChanged;
};
