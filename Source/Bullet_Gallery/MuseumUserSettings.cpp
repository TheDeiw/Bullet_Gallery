#include "MuseumUserSettings.h"

#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

UMuseumUserSettings::UMuseumUserSettings()
{
}

UMuseumUserSettings* UMuseumUserSettings::Get()
{
	return GEngine ? Cast<UMuseumUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UMuseumUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	ResetAudio();
	ResetGameplay();
	bMotionBlur = false;
	bFilmGrain = true;
}

void UMuseumUserSettings::ResetGraphics()
{
	SetOverallScalabilityLevel(2);
	ResolutionScalePercent = 100.f;
	ApplyResolutionScale();
	SetVSyncEnabled(true);
	SetFrameRateLimit(0.f);
	bMotionBlur = false;
	bFilmGrain = true;
}

void UMuseumUserSettings::ResetAudio()
{
	MasterVolume = 0.8f;
	MusicVolume = 0.6f;
	SfxVolume = 0.9f;
}

void UMuseumUserSettings::ResetGameplay()
{
	MouseSensitivity = 1.f;
	FieldOfView = 90.f;
	bInvertY = false;
	bCameraShake = true;
	bHeadBob = true;
	bDamageNumbers = true;
	bShowFPS = false;
}

void UMuseumUserSettings::ApplyResolutionScale()
{
	if (IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		ScreenPercentage->Set(FMath::Clamp(ResolutionScalePercent, 50.f, 100.f), ECVF_SetByGameSetting);
	}
}

void UMuseumUserSettings::ApplyAndSave()
{
	ApplySettings(false);
	SaveSettings();
	OnChanged.Broadcast();
}
