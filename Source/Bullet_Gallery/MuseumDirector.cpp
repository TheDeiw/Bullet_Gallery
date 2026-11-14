#include "MuseumDirector.h"

#include "DayNightCycle.h"
#include "Enemy.h"
#include "GuardNPC.h"
#include "MuseumSaveGame.h"
#include "MuseumStyle.h"
#include "MuseumUIWidget.h"
#include "MuseumUserSettings.h"
#include "PickupBase.h"
#include "PlayerCharacter.h"
#include "SpawnEnemyTargetPoint.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NavigationSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"

namespace
{
	/** Catches Escape, the gamepad Start button and the perk keys before the game or the widgets see them */
	class FMuseumInputProcessor : public IInputProcessor
	{
	public:
		explicit FMuseumInputProcessor(UMuseumDirector* InDirector) : Director(InDirector) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			UMuseumDirector* D = Director.Get();
			if (!D || InKeyEvent.IsRepeat()) { return false; }

			const FKey Key = InKeyEvent.GetKey();
			if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right)
			{
				D->OnBackPressed();
				return true;
			}
			if (Key == EKeys::F && D->IsGameplayActive() && D->GetPage() == EMuseumPage::None)
			{
				D->ToggleFlashlight();
				return false;
			}
			if (D->GetPage() == EMuseumPage::Perks)
			{
				if (Key == EKeys::One || Key == EKeys::NumPadOne) { D->ChoosePerk(0); return true; }
				if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { D->ChoosePerk(1); return true; }
				if (Key == EKeys::Three || Key == EKeys::NumPadThree) { D->ChoosePerk(2); return true; }
			}
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("MuseumInput"); }

	private:
		TWeakObjectPtr<UMuseumDirector> Director;
	};

	TSharedPtr<FMuseumInputProcessor> GInputProcessor;

	template <typename T, typename P>
	void FindAsset(TObjectPtr<P>& Out, const TCHAR* Path)
	{
		ConstructorHelpers::FObjectFinder<T> Finder(Path);
		if (Finder.Succeeded()) { Out = Finder.Object; }
	}

	template <typename T>
	void FindClass(TSubclassOf<AActor>& Out, const TCHAR* Path)
	{
		ConstructorHelpers::FClassFinder<T> Finder(Path);
		if (Finder.Succeeded()) { Out = Finder.Class; }
	}

	FPerkInfo MakePerk(const TCHAR* Name, const TCHAR* Description, const FLinearColor& Color)
	{
		return { FText::FromString(Name), FText::FromString(Description), Color };
	}
}

UMuseumDirector::UMuseumDirector()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		for (const TCHAR* Path : { TEXT("/Engine/EngineSounds/Master.Master"), TEXT("/Engine/EngineSounds/Music.Music"), TEXT("/Engine/EngineSounds/SFX.SFX") })
		{
			ConstructorHelpers::FObjectFinder<USoundClass> Finder(Path);
			if (Finder.Succeeded()) { CookedSoundClasses.Add(Finder.Object); }
		}
	}
}

void UMuseumDirector::LoadGameAssets()
{
	auto Class = [](const TCHAR* Path) -> TSubclassOf<AActor>
	{
		UClass* Loaded = StaticLoadClass(AActor::StaticClass(), nullptr, Path);
		if (!Loaded) { UE_LOG(LogTemp, Warning, TEXT("Museum director: class not found: %s"), Path); }
		return Loaded;
	};
	auto Sound = [](const TCHAR* Path) -> USoundBase*
	{
		USoundBase* Loaded = LoadObject<USoundBase>(nullptr, Path);
		if (!Loaded) { UE_LOG(LogTemp, Warning, TEXT("Museum director: sound not found: %s"), Path); }
		return Loaded;
	};

	WalkerClass = Class(TEXT("/Game/_MuseumProject/Enemies/BP_Enemy.BP_Enemy_C"));
	BronzeClass = Class(TEXT("/Game/_MuseumProject/Enemies/BP_Enemy_Bronze.BP_Enemy_Bronze_C"));
	GuardClass = Class(TEXT("/Game/_MuseumProject/AI/BP_GuardNPC.BP_GuardNPC_C"));
	HealthPickupClass = Class(TEXT("/Game/_MuseumProject/Interactables/BP_HealPickUp.BP_HealPickUp_C"));
	AmmoPickupClass = Class(TEXT("/Game/_MuseumProject/Interactables/BP_AmmoPickUp.BP_AmmoPickUp_C"));

	MusicSound = Sound(TEXT("/Game/_MuseumProject/Audio/SW_Music_Mansion.SW_Music_Mansion"));
	ClickSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_Click.SW_UI_Click"));
	HoverSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_Hover.SW_UI_Hover"));
	WaveStartSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_WaveStart.SW_UI_WaveStart"));
	WaveClearSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_WaveClear.SW_UI_WaveClear"));
	PerkSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_Perk.SW_UI_Perk"));
	GameOverSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_GameOver.SW_UI_GameOver"));
	VictorySound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_Victory.SW_UI_Victory"));
	HitMarkerSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_Hit.SW_UI_Hit"));
	KillSound = Sound(TEXT("/Game/_MuseumProject/Audio/UI/SW_UI_Kill.SW_UI_Kill"));

	// The engine's own sound classes; the project's sounds point to them, so they are cooked too
	MasterClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/Master.Master"));
	MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/Music.Music"));
	SfxClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/SFX.SFX"));
	if (!MasterClass || !MusicClass || !SfxClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Museum director: engine sound classes missing (master %d, music %d, sfx %d): volume sliders will not work"), MasterClass != nullptr, MusicClass != nullptr, SfxClass != nullptr);
	}
}

TStatId UMuseumDirector::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMuseumDirector, STATGROUP_Tickables);
}

UMuseumDirector* UMuseumDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UMuseumDirector>() : nullptr;
}

bool UMuseumDirector::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && World->GetMapName().Contains(TEXT("MuseumComplex"));
}

void UMuseumDirector::Deinitialize()
{
	if (GInputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(GInputProcessor);
	}
	GInputProcessor.Reset();
	MuseumUI::UISound = nullptr;
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------------------------

void UMuseumDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	LoadGameAssets();
	BuildWaves();
	BuildPerkInfos();
	PerkLevels.Init(0, (int32)EMuseumPerk::Count);
	Save = UMuseumSaveGame::Load();

	if (APlayerStart* Start = Cast<APlayerStart>(UGameplayStatics::GetActorOfClass(&InWorld, APlayerStart::StaticClass())))
	{
		PlayerStartLocation = Start->GetActorLocation();
		PlayerStartRotation = Start->GetActorRotation();
	}
	else if (APlayerCharacter* Player = GetPlayer())
	{
		PlayerStartLocation = Player->GetActorLocation();
		PlayerStartRotation = Player->GetActorRotation();
	}

	DayNight = Cast<ADayNightCycle>(UGameplayStatics::GetActorOfClass(&InWorld, ADayNightCycle::StaticClass()));
	CacheFallbackSpawns();

	MuseumUI::UISound = [this](bool bClick) { PlayUISound(bClick); };

	// The widget that holds the menus and the new HUD parts
	if (APlayerController* PC = GetPC())
	{
		UI = CreateWidget<UMuseumUIWidget>(PC, UMuseumUIWidget::StaticClass());
		if (UI)
		{
			UI->Setup(this);
			UI->AddToViewport(50);
		}
	}

	if (UMuseumUserSettings* Settings = UMuseumUserSettings::Get())
	{
		Settings->ApplyResolutionScale();
	}
	AssignSoundClasses();
	ApplyAudio();
	ApplyLook();
	if (UMuseumUserSettings* Settings = UMuseumUserSettings::Get())
	{
		Settings->OnChanged.AddWeakLambda(this, [this]() { ApplyAudio(); ApplyLook(); });
	}

	// Calm music for the whole session; it plays in the menu too (UI sound: not paused with the game)
	if (MusicSound)
	{
		MusicComponent = UGameplayStatics::CreateSound2D(&InWorld, MusicSound, 1.f, 1.f, 0.f, nullptr, false, false);
		if (MusicComponent)
		{
			MusicComponent->bIsUISound = true;
			MusicComponent->Play();
		}
	}

	if (FSlateApplication::IsInitialized())
	{
		GInputProcessor = MakeShared<FMuseumInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(GInputProcessor, 0);
	}

	if (UGameplayStatics::HasOption(InWorld.GetAuthGameMode() ? InWorld.GetAuthGameMode()->OptionsString : FString(), TEXT("SkipMenu")))
	{
		BeginRun();
	}
	else
	{
		EnterMenu();
	}
}

void UMuseumDirector::BuildWaves()
{
	auto Add = [this](const TCHAR* Title, const TCHAR* Sub, TArray<FWaveEntry> Entries, int32 MaxAlive, float Interval)
	{
		FWaveDef Def;
		Def.Title = FText::FromString(Title);
		Def.Subtitle = FText::FromString(Sub);
		Def.Entries = MoveTemp(Entries);
		Def.MaxAlive = MaxAlive;
		Def.SpawnInterval = Interval;
		Waves.Add(MoveTemp(Def));
	};

	using K = EEnemyKind;
	Add(TEXT("THE DOORS CREAK"), TEXT("The statues are waking up"),
		{ { K::Walker, 5 } }, 5, 1.8f);
	Add(TEXT("MARBLE AWAKENS"), TEXT("Something is running in the dark"),
		{ { K::Walker, 6 }, { K::Runner, 3 } }, 6, 1.5f);
	Add(TEXT("THE NIGHT SHIFT"), TEXT("The guards have joined in"),
		{ { K::Walker, 4 }, { K::Runner, 3 }, { K::Guard, 2 } }, 7, 1.5f);
	Add(TEXT("BRONZE AGE"), TEXT("Bullets sound different on metal"),
		{ { K::Bronze, 3 }, { K::Walker, 5 }, { K::Runner, 3 } }, 8, 1.4f);
	Add(TEXT("FAST AND ARMED"), TEXT("Keep moving"),
		{ { K::Runner, 7 }, { K::Guard, 3 }, { K::Bronze, 2 } }, 9, 1.2f);
	Add(TEXT("HEAVY EXHIBITS"), TEXT("The big ones are loose"),
		{ { K::Brute, 2 }, { K::Bronze, 3 }, { K::Walker, 6 }, { K::Guard, 2 } }, 9, 1.2f);
	Add(TEXT("NO WAY OUT"), TEXT("Dawn is only a few hours away"),
		{ { K::Brute, 2 }, { K::Runner, 8 }, { K::Guard, 4 }, { K::Bronze, 4 } }, 11, 1.0f);
	Add(TEXT("THE CURATOR"), TEXT("The exhibit that owns the museum"),
		{ { K::Boss, 1 }, { K::Walker, 6 }, { K::Guard, 3 }, { K::Runner, 6 } }, 11, 1.2f);
}

void UMuseumDirector::BuildPerkInfos()
{
	// Static table, see GetPerkInfo
}

const FPerkInfo& UMuseumDirector::GetPerkInfo(EMuseumPerk Perk)
{
	static const TArray<FPerkInfo> Infos =
	{
		MakePerk(TEXT("HOLLOW POINTS"), TEXT("Your bullets hit 20% harder."), FLinearColor(0.95f, 0.45f, 0.25f)),
		MakePerk(TEXT("HAIR TRIGGER"), TEXT("Fire 15% faster."), FLinearColor(0.95f, 0.8f, 0.3f)),
		MakePerk(TEXT("SPEED LOADER"), TEXT("Reload 25% faster."), FLinearColor(0.4f, 0.75f, 0.95f)),
		MakePerk(TEXT("AMMO SATCHEL"), TEXT("Carry 30 more spare rounds and get 30 right now."), FLinearColor(0.85f, 0.7f, 0.4f)),
		MakePerk(TEXT("IRON CONSTITUTION"), TEXT("+25 maximum health and heal 40."), FLinearColor(0.85f, 0.3f, 0.3f)),
		MakePerk(TEXT("RUNNING SHOES"), TEXT("Move 8% faster."), FLinearColor(0.45f, 0.85f, 0.55f)),
		MakePerk(TEXT("LEECH ROUNDS"), TEXT("Heal 3 health for every kill."), FLinearColor(0.75f, 0.35f, 0.85f)),
		MakePerk(TEXT("SCAVENGER"), TEXT("Enemies drop medkits and ammo more often."), FLinearColor(0.6f, 0.8f, 0.3f)),
	};
	return Infos[FMath::Clamp((int32)Perk, 0, Infos.Num() - 1)];
}

void UMuseumDirector::CacheFallbackSpawns()
{
	TArray<AActor*> Points;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASpawnEnemyTargetPoint::StaticClass(), Points);
	for (const AActor* Point : Points)
	{
		FallbackSpawns.Add(Point->GetActorLocation());
	}
}

void UMuseumDirector::AssignSoundClasses()
{
	// The game's sounds are sorted into two classes so that the music and the effects have their own volume sliders
	if (!MusicClass || !SfxClass) { return; }
	for (TObjectIterator<USoundWave> It; It; ++It)
	{
		USoundWave* SoundWave = *It;
		if (!SoundWave->GetPathName().StartsWith(TEXT("/Game/_MuseumProject"))) { continue; }
		const bool bMusic = SoundWave->GetName().Contains(TEXT("Music"));
		SoundWave->SoundClassObject = bMusic ? MusicClass.Get() : SfxClass.Get();
	}
}

void UMuseumDirector::ApplyAudio()
{
	UWorld* World = GetWorld();
	const UMuseumUserSettings* Settings = UMuseumUserSettings::Get();
	if (!World || !Settings || !MasterClass || !MusicClass || !SfxClass) { return; }

	if (!Mix)
	{
		Mix = NewObject<USoundMix>(this, TEXT("MuseumMix"));
		UGameplayStatics::PushSoundMixModifier(World, Mix);
	}
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, MasterClass, Settings->MasterVolume, 1.f, 0.f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, MusicClass, Settings->MusicVolume, 1.f, 0.f, true);
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, SfxClass, Settings->SfxVolume, 1.f, 0.f, true);
}

void UMuseumDirector::ApplyLook()
{
	UWorld* World = GetWorld();
	const UMuseumUserSettings* Settings = UMuseumUserSettings::Get();
	if (!World) { return; }

	if (!LookVolume)
	{
		LookVolume = World->SpawnActor<APostProcessVolume>();
		if (!LookVolume) { return; }
		LookVolume->bUnbound = true;
		LookVolume->Priority = 100.f;
		LookVolume->BlendWeight = 1.f;
	}

	FPostProcessSettings& PP = LookVolume->Settings;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.42f;
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.5f;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.75f;
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = (!Settings || Settings->bFilmGrain) ? 0.16f : 0.f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = (Settings && Settings->bMotionBlur) ? 0.45f : 0.f;
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.f);
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(1.08f, 1.08f, 1.08f, 1.f);
	// The museum garden is lit by a few lamps: lift the exposure a little so the night stays readable
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.6f;
}

void UMuseumDirector::UpdateLook(float RealDelta)
{
	if (!LookVolume) { return; }

	// Low health: the picture drains of colour and the edges darken; every hit adds a short pulse
	const APlayerCharacter* Player = GetPlayer();
	const float Fraction = (Player && Flow != EMuseumFlow::Menu) ? Player->GetHealth() / FMath::Max(Player->GetMaxHealth(), 1.f) : 1.f;
	const float TargetLow = FMath::GetMappedRangeValueClamped(FVector2D(0.45f, 0.1f), FVector2D(0.f, 1.f), Fraction);
	LowHealthBlend = FMath::FInterpTo(LowHealthBlend, TargetLow, RealDelta, 3.f);
	HurtPulse = FMath::FInterpTo(HurtPulse, 0.f, RealDelta, 5.f);

	FPostProcessSettings& PP = LookVolume->Settings;
	const float Sat = FMath::Lerp(1.08f, 0.55f, LowHealthBlend);
	PP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	PP.VignetteIntensity = 0.42f + 0.35f * LowHealthBlend + 0.25f * HurtPulse;
	PP.SceneFringeIntensity = 0.5f + 2.5f * HurtPulse + 1.f * LowHealthBlend;
}

// ---------------------------------------------------------------------------------------------------------------
// Flow
// ---------------------------------------------------------------------------------------------------------------

APlayerCharacter* UMuseumDirector::GetPlayer() const
{
	return Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}

APlayerController* UMuseumDirector::GetPC() const
{
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

EMuseumPage UMuseumDirector::GetPage() const
{
	return UI ? UI->GetPage() : EMuseumPage::None;
}

void UMuseumDirector::PlayUISound(bool bClick)
{
	USoundBase* Sound = bClick ? ClickSound.Get() : HoverSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound, bClick ? 0.9f : 0.5f, 1.f, 0.f, nullptr, nullptr, true);
	}
}

void UMuseumDirector::SetFlow(EMuseumFlow NewFlow)
{
	Flow = NewFlow;

	// Health and ammo are part of the game, not of the menus
	if (APlayerCharacter* Player = GetPlayer())
	{
		Player->SetHUDVisible(NewFlow == EMuseumFlow::Playing || NewFlow == EMuseumFlow::Intermission || NewFlow == EMuseumFlow::Paused);
	}
}

void UMuseumDirector::SetMenuMode(bool bMenuOpen)
{
	APlayerController* PC = GetPC();
	UWorld* World = GetWorld();
	if (!PC || !World) { return; }

	UGameplayStatics::SetGamePaused(World, bMenuOpen);
	PC->bShowMouseCursor = bMenuOpen;
	PC->bEnableClickEvents = bMenuOpen;
	PC->bEnableMouseOverEvents = bMenuOpen;

	if (bMenuOpen)
	{
		FInputModeUIOnly Mode;
		if (UI) { Mode.SetWidgetToFocus(UI->TakeWidget()); }
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->FlushPressedKeys();
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void UMuseumDirector::EnterMenu()
{
	SetFlow(EMuseumFlow::Menu);
	bRunStarted = false;
	bPlayerDead = false;

	DestroyAllEnemies();
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);

	if (APlayerCharacter* Player = GetPlayer())
	{
		Player->ResetForRun();
	}

	if (DayNight)
	{
		DayNight->SetExternalControl(true);
		ApplyHour(17.4f);
	}

	// A slow camera flight over the museum; the world itself is frozen behind the menu
	if (!MenuCamera)
	{
		MenuCamera = GetWorld()->SpawnActor<ACameraActor>(PlayerStartLocation, PlayerStartRotation);
		if (MenuCamera)
		{
			MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		}
	}
	if (MenuShots.Num() == 0)
	{
		// Slow flights over the garden in front of the museum (hand-picked for L_MuseumComplex)
		const FVector Facade(3400.f, -300.f, 520.f);
		const FVector Garden(3300.f, -700.f, 260.f);
		MenuShots.Add({ FVector(6300.f, -2500.f, 520.f), FVector(5400.f, -2300.f, 420.f), Garden, 16.f, 60.f });
		MenuShots.Add({ FVector(5400.f, -2200.f, 230.f), FVector(3300.f, -2400.f, 240.f), Facade, 16.f, 60.f });
	}
	MenuShotIndex = 0;
	MenuShotTime = 0.f;
	UpdateMenuCamera(0.f);
	if (APlayerController* PC = GetPC())
	{
		if (MenuCamera) { PC->SetViewTargetWithBlend(MenuCamera, 0.f); }
	}

	if (UI) { UI->ShowPage(EMuseumPage::Main); }
	SetMenuMode(true);
}

void UMuseumDirector::UpdateMenuCamera(float RealDelta)
{
	if (!MenuCamera || MenuShots.Num() == 0 || bFreeCamera) { return; }

	MenuShotTime += RealDelta;
	const FMenuShot& Shot = MenuShots[MenuShotIndex % MenuShots.Num()];
	if (MenuShotTime >= Shot.Duration)
	{
		MenuShotTime = 0.f;
		MenuShotIndex = (MenuShotIndex + 1) % MenuShots.Num();
		return;
	}

	const float Alpha = FMath::SmoothStep(0.f, 1.f, MenuShotTime / Shot.Duration);
	const FVector Location = FMath::Lerp(Shot.From, Shot.To, Alpha);
	MenuCamera->SetActorLocationAndRotation(Location, (Shot.LookAt - Location).Rotation());
	MenuCamera->GetCameraComponent()->SetFieldOfView(Shot.Fov);
}

void UMuseumDirector::DebugSetMenuShot(int32 Index)
{
	if (MenuShots.Num() == 0) { return; }
	MenuShotIndex = Index % MenuShots.Num();
	MenuShotTime = 0.5f;
}

void UMuseumDirector::StartGame()
{
	BeginRun();
}

void UMuseumDirector::BeginRun()
{
	UWorld* World = GetWorld();
	APlayerCharacter* Player = GetPlayer();
	if (!World || !Player) { return; }

	// Back to the player's eyes and a fresh start
	DestroyAllEnemies();
	UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	Player->ResetForRun();
	Player->SetActorLocationAndRotation(PlayerStartLocation + FVector(0.f, 0.f, 10.f), PlayerStartRotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* PC = GetPC())
	{
		PC->SetControlRotation(PlayerStartRotation);
		PC->SetViewTargetWithBlend(Player, bRunStarted ? 0.f : 1.4f, VTBlend_Cubic);
	}

	Score = 0;
	Kills = 0;
	Headshots = 0;
	Combo = 0;
	ComboTimeLeft = 0.f;
	RunTime = 0.f;
	DamageTaken = 0.f;
	bNewBest = false;
	bPlayerDead = false;
	bRunStarted = true;
	DayBreaks = false;
	bHourLocked = false;
	PerkLevels.Init(0, (int32)EMuseumPerk::Count);
	Feed.Reset();

	if (Save)
	{
		Save->GamesPlayed++;
		Save->Write();
	}

	if (DayNight) { DayNight->SetExternalControl(true); }
	SmoothHour = 17.6f;
	ApplyHour(SmoothHour);

	if (UI) { UI->ShowPage(EMuseumPage::None); }
	SetMenuMode(false);
	BeginWave(1);
}

void UMuseumDirector::Resume()
{
	if (Flow != EMuseumFlow::Paused) { return; }

	SetFlow(FlowBeforeSettings);
	if (UI) { UI->ShowPage(Flow == EMuseumFlow::Intermission && bPerkShown ? EMuseumPage::Perks : EMuseumPage::None); }
	SetMenuMode(UI && UI->GetPage() == EMuseumPage::Perks);
}

void UMuseumDirector::RestartRun()
{
	BeginRun();
}

void UMuseumDirector::ReturnToMenu()
{
	EnterMenu();
}

void UMuseumDirector::QuitGame()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), GetPC(), EQuitPreference::Quit, false);
}

void UMuseumDirector::ShowSettings()
{
	if (UI) { UI->ShowPage(EMuseumPage::Settings); }
}

void UMuseumDirector::ShowHowTo()
{
	if (UI) { UI->ShowPage(EMuseumPage::HowTo); }
}

void UMuseumDirector::ToggleFlashlight()
{
	if (APlayerCharacter* Player = GetPlayer()) { Player->ToggleFlashlight(); }
}

void UMuseumDirector::CloseSettings()
{
	if (UMuseumUserSettings* Settings = UMuseumUserSettings::Get())
	{
		Settings->ApplyAndSave();
	}
	if (UI) { UI->ShowPage(Flow == EMuseumFlow::Menu ? EMuseumPage::Main : EMuseumPage::Pause); }
}

void UMuseumDirector::OnBackPressed()
{
	const EMuseumPage Page = GetPage();
	switch (Flow)
	{
	case EMuseumFlow::Playing:
	case EMuseumFlow::Intermission:
		// While the perk cards are up the choice must be made
		if (Flow == EMuseumFlow::Intermission && Page == EMuseumPage::Perks) { break; }
		if (!bPlayerDead)
		{
			FlowBeforeSettings = Flow;
			SetFlow(EMuseumFlow::Paused);
			if (UI) { UI->ShowPage(EMuseumPage::Pause); }
			SetMenuMode(true);
		}
		break;
	case EMuseumFlow::Paused:
		if (Page == EMuseumPage::Settings) { CloseSettings(); }
		else if (Page == EMuseumPage::HowTo) { if (UI) { UI->ShowPage(EMuseumPage::Pause); } }
		else { Resume(); }
		break;
	case EMuseumFlow::Menu:
		if (Page == EMuseumPage::Settings) { CloseSettings(); }
		else if (Page == EMuseumPage::HowTo) { if (UI) { UI->ShowPage(EMuseumPage::Main); } }
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Waves
// ---------------------------------------------------------------------------------------------------------------

void UMuseumDirector::BeginWave(int32 WaveNumber)
{
	Wave = FMath::Clamp(WaveNumber, 1, Waves.Num());
	SetFlow(EMuseumFlow::Playing);
	WaveDamageTaken = 0.f;
	TimeSinceKill = 0.f;
	HintTimer = (Wave == 1 && Save && Save->GamesPlayed <= 1) ? 4.2f : 0.f;

	const FWaveDef& Def = Waves[Wave - 1];
	Pending.Reset();
	for (const FWaveEntry& Entry : Def.Entries)
	{
		for (int32 i = 0; i < Entry.Count; ++i) { Pending.Add(Entry.Kind); }
	}
	// Mixed order, but the boss opens the last wave
	for (int32 i = Pending.Num() - 1; i > 0; --i)
	{
		Pending.Swap(i, FMath::RandRange(0, i));
	}
	if (Def.Entries.Num() > 0 && Def.Entries[0].Kind == EEnemyKind::Boss)
	{
		const int32 BossIndex = Pending.Find(EEnemyKind::Boss);
		if (BossIndex != INDEX_NONE) { Pending.Swap(BossIndex, Pending.Num() - 1); }
	}
	WaveTotal = Pending.Num();
	SpawnTimer = 2.2f;
	Alive.Reset();
	Boss.Reset();

	ShowBanner(FText::FromString(FString::Printf(TEXT("WAVE %d"), Wave)), FText::FromString(Def.Title.ToString() + TEXT("  -  ") + Def.Subtitle.ToString()), MuseumUI::Colors::GoldHi, 3.2f);
	if (WaveStartSound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), WaveStartSound, 1.f, 1.f, 0.f, nullptr, nullptr, true);
	}
}

int32 UMuseumDirector::GetEnemiesLeft() const
{
	int32 Count = Pending.Num();
	for (const TWeakObjectPtr<AActor>& Enemy : Alive)
	{
		if (Enemy.IsValid()) { ++Count; }
	}
	return Count;
}

FEnemyVariant UMuseumDirector::MakeVariant(EEnemyKind Kind) const
{
	FEnemyVariant V;
	V.Kind = Kind;
	switch (Kind)
	{
	case EEnemyKind::Walker:
		V.Name = FText::FromString(TEXT("MARBLE STATUE"));
		V.Points = 100;
		break;
	case EEnemyKind::Runner:
		V.Name = FText::FromString(TEXT("SPRINTER"));
		V.HealthMul = 0.55f; V.SpeedMul = 1.45f; V.DamageMul = 0.75f; V.Scale = 0.93f;
		V.Points = 130;
		V.Aura = FLinearColor(0.25f, 0.8f, 1.f, 1.f);
		break;
	case EEnemyKind::Bronze:
		V.Name = FText::FromString(TEXT("BRONZE SENTINEL"));
		V.HealthMul = 1.6f; V.SpeedMul = 0.95f; V.DamageMul = 1.2f;
		V.Points = 250;
		V.Aura = FLinearColor(1.f, 0.55f, 0.15f, 1.f);
		break;
	case EEnemyKind::Brute:
		V.Name = FText::FromString(TEXT("STONE BRUTE"));
		V.HealthMul = 3.2f; V.SpeedMul = 0.72f; V.DamageMul = 2.2f; V.Scale = 1.55f;
		V.Points = 500;
		V.Aura = FLinearColor(1.f, 0.2f, 0.12f, 1.f);
		break;
	case EEnemyKind::Guard:
		V.Name = FText::FromString(TEXT("NIGHT GUARD"));
		V.Points = 200;
		break;
	case EEnemyKind::Boss:
		V.Name = FText::FromString(TEXT("THE CURATOR"));
		V.HealthMul = 11.f; V.SpeedMul = 0.88f; V.DamageMul = 3.f; V.Scale = 2.1f;
		V.Points = 5000;
		V.Aura = FLinearColor(1.f, 0.78f, 0.25f, 1.f);
		break;
	}

	// The museum gets meaner as the night goes on
	const float Progress = FMath::Max(Wave - 1, 0);
	V.HealthMul *= 1.f + 0.07f * Progress;
	V.DamageMul *= 1.f + 0.04f * Progress;
	return V;
}

bool UMuseumDirector::FindSpawnLocation(FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	const APlayerCharacter* Player = GetPlayer();
	if (!World || !Player) { return false; }

	const FVector PlayerLocation = Player->GetActorLocation();
	const FVector Eyes = PlayerLocation + FVector(0.f, 0.f, 60.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SpawnVisible), false, Player);

	// The garden in front of the museum (inside the invisible walls of L_MuseumComplex)
	const FBox2D Garden(FVector2D(300.f, -4200.f), FVector2D(6500.f, -250.f));
	const bool bPlayerInGarden = Garden.IsInside(FVector2D(PlayerLocation));

	auto Acceptable = [&](const FVector& Candidate, bool bNeedHidden)
	{
		const float Distance = FVector::Dist2D(Candidate, PlayerLocation);
		if (Distance < 1300.f || FMath::Abs(Candidate.Z - PlayerLocation.Z) > 350.f) { return false; }
		// Not in plain sight of the player: enemies appear in the dark, behind corners and trees
		return !bNeedHidden || World->LineTraceTestByChannel(Eyes, Candidate + FVector(0.f, 0.f, 110.f), ECC_Visibility, Params);
	};

	// 1. Inside the museum: a random point of the navigation mesh that the player can be reached from
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation Projected;
	if (Nav && Nav->ProjectPointToNavigation(PlayerLocation, Projected, FVector(150.f, 150.f, 300.f)))
	{
		for (int32 Try = 0; Try < 30; ++Try)
		{
			FNavLocation Point;
			if (Nav->GetRandomReachablePointInRadius(PlayerLocation, 4200.f, Point) && Acceptable(Point.Location, Try < 20))
			{
				OutLocation = Point.Location;
				return true;
			}
		}
	}

	// 2. Garden (or anywhere without a navigation mesh): a ring around the player, the floor found with a trace
	const FCollisionShape Body = FCollisionShape::MakeCapsule(40.f, 90.f);
	for (int32 Try = 0; Try < 60; ++Try)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Distance = FMath::FRandRange(1500.f, 3600.f);
		const FVector Guess = PlayerLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
		if (bPlayerInGarden && !Garden.IsInside(FVector2D(Guess))) { continue; }

		FHitResult Floor;
		if (!World->LineTraceSingleByChannel(Floor, Guess + FVector(0.f, 0.f, 500.f), Guess - FVector(0.f, 0.f, 700.f), ECC_Visibility, Params)) { continue; }
		if (Floor.ImpactNormal.Z < 0.85f) { continue; }
		const FVector Spot = Floor.ImpactPoint;
		if (World->OverlapBlockingTestByChannel(Spot + FVector(0.f, 0.f, 95.f), FQuat::Identity, ECC_Pawn, Body, Params)) { continue; }
		if (Acceptable(Spot, Try < 40 && bPlayerInGarden))
		{
			OutLocation = Spot;
			return true;
		}
	}

	// 3. The museum's own spawn points
	TArray<FVector> Candidates;
	for (const FVector& Point : FallbackSpawns)
	{
		if (Acceptable(Point, false)) { Candidates.Add(Point); }
	}
	if (Candidates.Num() > 0)
	{
		OutLocation = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
		return true;
	}
	return false;
}

AActor* UMuseumDirector::SpawnEnemy(EEnemyKind Kind)
{
	FVector Location;
	if (!GetPlayer() || !FindSpawnLocation(Location)) { return nullptr; }
	return SpawnEnemyAt(Kind, Location, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), true);
}

AActor* UMuseumDirector::SpawnEnemyAt(EEnemyKind Kind, const FVector& FloorLocation, const FRotator& Rotation, bool bHunter)
{
	UWorld* World = GetWorld();
	APlayerCharacter* Player = GetPlayer();
	if (!World || !Player) { return nullptr; }

	FEnemyVariant Variant = MakeVariant(Kind);
	Variant.bHunter = bHunter;
	const bool bGuard = Kind == EEnemyKind::Guard;
	const bool bMetal = Kind == EEnemyKind::Bronze || Kind == EEnemyKind::Boss;
	UClass* Class = bGuard ? GuardClass.Get() : (bMetal && BronzeClass ? BronzeClass.Get() : WalkerClass.Get());
	if (!Class) { return nullptr; }

	const ACharacter* Defaults = Class->GetDefaultObject<ACharacter>();
	const float HalfHeight = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * Variant.Scale;
	FTransform Transform(Rotation, FloorLocation + FVector(0.f, 0.f, HalfHeight + 8.f), FVector(Variant.Scale));

	AActor* Spawned = World->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Spawned) { return nullptr; }

	if (AEnemy* Enemy = Cast<AEnemy>(Spawned))
	{
		Enemy->ApplyVariant(Variant);
	}
	else if (AGuardNPC* Guard = Cast<AGuardNPC>(Spawned))
	{
		Guard->ApplyVariant(Variant);
	}
	UGameplayStatics::FinishSpawningActor(Spawned, Transform);

	if (bHunter)
	{
		if (AGuardNPC* Guard = Cast<AGuardNPC>(Spawned))
		{
			Guard->AlertTo(Player);
		}
		if (Kind == EEnemyKind::Boss) { Boss = Spawned; }
	}
	return Spawned;
}

void UMuseumDirector::SpawnNext()
{
	if (Pending.Num() == 0) { return; }

	const EEnemyKind Kind = Pending.Last();
	if (AActor* Enemy = SpawnEnemy(Kind))
	{
		Pending.Pop();
		Alive.Add(Enemy);
		SpawnTimer = GetWaveDef().SpawnInterval;
		if (Kind == EEnemyKind::Boss)
		{
			ShowBanner(FText::FromString(TEXT("THE CURATOR")), FText::FromString(TEXT("It has been waiting for someone to visit")), MuseumUI::Colors::Red, 3.f);
		}
	}
	else
	{
		// No free place right now: try again soon
		SpawnTimer = 0.8f;
		UE_LOG(LogTemp, Warning, TEXT("Museum director: could not spawn kind %d (classes: walker %s, bronze %s, guard %s)"), (int32)Kind,
			WalkerClass ? TEXT("ok") : TEXT("MISSING"), BronzeClass ? TEXT("ok") : TEXT("MISSING"), GuardClass ? TEXT("ok") : TEXT("MISSING"));
	}
}

void UMuseumDirector::DestroyAllEnemies()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemy::StaticClass(), Actors);
	for (AActor* Actor : Actors) { Actor->Destroy(); }
	Actors.Reset();
	UGameplayStatics::GetAllActorsOfClass(World, AGuardNPC::StaticClass(), Actors);
	for (AActor* Actor : Actors) { Actor->Destroy(); }

	// Loot dropped earlier and bullets still flying
	Actors.Reset();
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("DroppedLoot"), Actors);
	for (AActor* Actor : Actors) { Actor->Destroy(); }

	Alive.Reset();
	Pending.Reset();
	Boss.Reset();
}

void UMuseumDirector::DropLoot(const FVector& Location, EEnemyKind Kind)
{
	UWorld* World = GetWorld();
	const APlayerCharacter* Player = GetPlayer();
	if (!World || !Player) { return; }

	float Chance = 0.16f;
	int32 Drops = 1;
	switch (Kind)
	{
	case EEnemyKind::Runner: Chance = 0.12f; break;
	case EEnemyKind::Bronze: Chance = 0.32f; break;
	case EEnemyKind::Brute: Chance = 0.6f; break;
	case EEnemyKind::Guard: Chance = 0.38f; break;
	case EEnemyKind::Boss: Chance = 1.f; Drops = 3; break;
	default: break;
	}
	Chance += 0.07f * GetPerkLevel(EMuseumPerk::Scavenger);
	// A player in trouble gets help more often
	if (Player->GetHealth() < 35.f || Player->GetReserveAmmo() < 12) { Chance += 0.2f; }

	for (int32 i = 0; i < Drops; ++i)
	{
		if (FMath::FRand() > Chance) { continue; }

		const bool bWantsHealth = Player->GetHealth() < Player->GetMaxHealth() * 0.55f;
		const bool bWantsAmmo = Player->GetReserveAmmo() < 24;
		const bool bHealth = bWantsHealth && !bWantsAmmo ? true : (bWantsAmmo && !bWantsHealth ? false : FMath::RandBool());
		UClass* Class = bHealth ? HealthPickupClass.Get() : AmmoPickupClass.Get();
		if (!Class) { continue; }

		// Land on the floor under the corpse
		FVector Spot = Location + FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), 0.f);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Spot, Spot - FVector(0.f, 0.f, 600.f), ECC_Visibility))
		{
			Spot = Hit.ImpactPoint;
		}
		if (AActor* Loot = World->SpawnActor<AActor>(Class, Spot + FVector(0.f, 0.f, 45.f), FRotator::ZeroRotator))
		{
			Loot->Tags.Add(TEXT("DroppedLoot"));
			Loot->SetLifeSpan(35.f);
		}
	}
}

void UMuseumDirector::NotifyEnemyKilled(AActor* Enemy, const FEnemyVariant& Variant, bool bHeadshot)
{
	if (Flow != EMuseumFlow::Playing && Flow != EMuseumFlow::Intermission) { return; }

	Alive.RemoveAll([Enemy](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid() || Entry.Get() == Enemy; });
	TimeSinceKill = 0.f;

	++Kills;
	Combo = ComboTimeLeft > 0.f ? Combo + 1 : 1;
	ComboTimeLeft = ComboWindow;
	const int32 Multiplier = GetMultiplier();

	int32 Points = Variant.Points;
	if (bHeadshot)
	{
		++Headshots;
		Points += 50;
	}
	Score += Points * Multiplier;

	FString Text = FString::Printf(TEXT("+%d  %s"), Points * Multiplier, *Variant.Name.ToString());
	if (bHeadshot) { Text += TEXT("  HEADSHOT"); }
	PushFeed(FText::FromString(Text), bHeadshot ? MuseumUI::Colors::GoldHi : MuseumUI::Colors::Text);

	if (APlayerCharacter* Player = GetPlayer())
	{
		const int32 Leech = GetPerkLevel(EMuseumPerk::Leech) * 3;
		if (Leech > 0) { Player->AddHealth(Leech); }
	}
	DropLoot(Enemy->GetActorLocation(), Variant.Kind);

	if (Variant.Kind == EEnemyKind::Boss)
	{
		HitStop(0.7f, 0.06f);
	}
	else if (Variant.Kind == EEnemyKind::Brute || (Pending.Num() == 0 && Alive.Num() == 0))
	{
		HitStop(0.22f, 0.12f);
	}
}

void UMuseumDirector::NotifyPlayerHurt(float Damage)
{
	DamageTaken += Damage;
	WaveDamageTaken += Damage;
	HurtPulse = FMath::Min(HurtPulse + 0.8f, 1.f);
	// Getting hit breaks the combo
	ComboTimeLeft = FMath::Min(ComboTimeLeft, 1.f);
}

void UMuseumDirector::PlayHitFeedback(bool bHeadshot, bool bKilled)
{
	USoundBase* Sound = bKilled && KillSound ? KillSound.Get() : HitMarkerSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound, bHeadshot ? 0.9f : 0.6f, bHeadshot ? 1.25f : 1.f, 0.f, nullptr, nullptr, true);
	}
}

void UMuseumDirector::NotifyPlayerDied()
{
	if (bPlayerDead) { return; }
	bPlayerDead = true;
	DeadTimer = 1.8f;
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.3f);
}

void UMuseumDirector::HitStop(float RealSeconds, float Dilation)
{
	if (bPlayerDead || Flow == EMuseumFlow::Paused) { return; }
	HitStopLeft = RealSeconds;
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), Dilation);
}

void UMuseumDirector::WaveCleared()
{
	const int32 Bonus = 400 * Wave + (WaveDamageTaken <= 0.f ? 1000 : 0);
	Score += Bonus;

	const bool bFinal = Wave >= Waves.Num();
	FString Sub = FString::Printf(TEXT("+%d bonus"), Bonus);
	if (WaveDamageTaken <= 0.f) { Sub += TEXT("  -  FLAWLESS"); }
	ShowBanner(FText::FromString(bFinal ? TEXT("THE NIGHT IS OVER") : FString::Printf(TEXT("WAVE %d CLEARED"), Wave)), FText::FromString(Sub), MuseumUI::Colors::Green, 3.f);
	if (WaveClearSound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), WaveClearSound, 1.f, 1.f, 0.f, nullptr, nullptr, true);
	}

	SetFlow(EMuseumFlow::Intermission);
	bPerkShown = false;
	IntermissionLeft = 0.f;

	if (APlayerCharacter* Player = GetPlayer())
	{
		// The museum staff left some supplies
		Player->AddHealth(25.f);
		Player->AddAmmo(25);
		Player->ShowHUDMessage(FText::FromString(TEXT("Supplies restocked")));
	}
	PerkDelay = bFinal ? 3.5f : 2.4f;
	if (bFinal) { DayBreaks = true; }
}

void UMuseumDirector::ShowPerkChoice()
{
	// Three different perks that are not maxed out yet
	TArray<EMuseumPerk> Pool;
	for (int32 i = 0; i < (int32)EMuseumPerk::Count; ++i)
	{
		if (PerkLevels[i] < 5) { Pool.Add((EMuseumPerk)i); }
	}
	Offers.Reset();
	while (Offers.Num() < 3 && Pool.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, Pool.Num() - 1);
		Offers.Add(Pool[Index]);
		Pool.RemoveAtSwap(Index);
	}

	bPerkShown = true;
	if (UI) { UI->ShowPage(EMuseumPage::Perks); }
	SetMenuMode(true);
}

void UMuseumDirector::ChoosePerk(int32 Index)
{
	if (Flow != EMuseumFlow::Intermission || !bPerkShown || !Offers.IsValidIndex(Index)) { return; }

	const EMuseumPerk Perk = Offers[Index];
	PerkLevels[(int32)Perk]++;
	if (APlayerCharacter* Player = GetPlayer())
	{
		Player->ApplyPerk(Perk);
	}
	if (PerkSound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), PerkSound, 1.f, 1.f, 0.f, nullptr, nullptr, true);
	}

	bPerkShown = false;
	if (UI) { UI->ShowPage(EMuseumPage::None); }
	SetMenuMode(false);
	IntermissionLeft = 9.f;
	ShowBanner(FText::FromString(GetPerkInfo(Perk).Name.ToString()), FText::FromString(GetPerkInfo(Perk).Description.ToString()), GetPerkInfo(Perk).Color, 2.6f);
}

void UMuseumDirector::FinishIntermission()
{
	BeginWave(Wave + 1);
}

void UMuseumDirector::SaveProgress(bool bVictory)
{
	if (!Save) { return; }

	bNewBest = Score > Save->BestScore;
	Save->BestScore = FMath::Max(Save->BestScore, Score);
	Save->BestWave = FMath::Max(Save->BestWave, bVictory ? Waves.Num() : Wave);
	Save->TotalKills += Kills;
	if (bVictory)
	{
		Save->Victories++;
		Save->BestVictoryTime = Save->BestVictoryTime <= 0.f ? RunTime : FMath::Min(Save->BestVictoryTime, RunTime);
	}
	Save->Write();
}

FString UMuseumDirector::GetRank() const
{
	if (Score >= 50000) { return TEXT("S"); }
	if (Score >= 35000) { return TEXT("A"); }
	if (Score >= 22000) { return TEXT("B"); }
	if (Score >= 10000) { return TEXT("C"); }
	return TEXT("D");
}

void UMuseumDirector::EndRun(bool bVictory)
{
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
	HitStopLeft = 0.f;
	SaveProgress(bVictory);
	SetFlow(bVictory ? EMuseumFlow::Victory : EMuseumFlow::GameOver);

	USoundBase* Sound = bVictory ? VictorySound.Get() : GameOverSound.Get();
	if (Sound) { UGameplayStatics::PlaySound2D(GetWorld(), Sound, 1.f, 1.f, 0.f, nullptr, nullptr, true); }

	if (UI) { UI->ShowPage(bVictory ? EMuseumPage::Victory : EMuseumPage::GameOver); }
	SetMenuMode(true);
}

int32 UMuseumDirector::GetBestScore() const
{
	return Save ? FMath::Max(Save->BestScore, Score) : Score;
}

int32 UMuseumDirector::GetMultiplier() const
{
	return FMath::Clamp(1 + (Combo - 1) / 2, 1, 6);
}

float UMuseumDirector::GetBossHealth() const
{
	if (const AEnemy* BossEnemy = Cast<AEnemy>(Boss.Get()))
	{
		return BossEnemy->GetHealthFraction();
	}
	return 0.f;
}

void UMuseumDirector::ShowBanner(const FText& Title, const FText& Sub, const FLinearColor& Color, float Duration)
{
	BannerTitle = Title;
	BannerSub = Sub;
	BannerColor = Color;
	BannerAge = 0.f;
	BannerDuration = Duration;
}

void UMuseumDirector::PushFeed(const FText& Text, const FLinearColor& Color)
{
	Feed.Insert({ Text, Color, 0.f }, 0);
	if (Feed.Num() > 5) { Feed.SetNum(5); }
}

void UMuseumDirector::ApplyHour(float Hour)
{
	if (DayNight) { DayNight->SetHour(Hour); }
}

// ---------------------------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------------------------

void UMuseumDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Real time: not scaled by the hit stop and still running while the game is paused
	const double Now = FPlatformTime::Seconds();
	const float RealDelta = LastRealTime > 0.0 ? FMath::Min(float(Now - LastRealTime), 0.1f) : 0.f;
	LastRealTime = Now;

	if (APlayerCharacter* HudPlayer = GetPlayer())
	{
		HudPlayer->SetHUDVisible(!bUiHidden && (Flow == EMuseumFlow::Playing || Flow == EMuseumFlow::Intermission || Flow == EMuseumFlow::Paused));
	}

	StartupTime += RealDelta;
	HandleCommandQueue(RealDelta);
	if (UI) { UI->TickUI(RealDelta); }
	UpdateLook(RealDelta);

	if (DebugShotsLeft > 0 && Flow == EMuseumFlow::Playing)
	{
		DebugShotTimer -= RealDelta;
		if (DebugShotTimer <= 0.f)
		{
			DebugShotTimer = 0.5f;
			--DebugShotsLeft;
			if (APlayerCharacter* ShotPlayer = GetPlayer()) { ShotPlayer->bDebugFire = true; }
		}
	}

	if (HintTimer > 0.f && Flow == EMuseumFlow::Playing)
	{
		HintTimer -= RealDelta;
		if (HintTimer <= 0.f)
		{
			ShowBanner(FText::FromString(TEXT("HEADS UP")), FText::FromString(TEXT("Aim for the head for double damage  -  F toggles your flashlight")), MuseumUI::Colors::Text, 5.f);
		}
	}

	BannerAge += RealDelta;
	for (FFeedEntry& Entry : Feed) { Entry.Age += RealDelta; }
	Feed.RemoveAll([](const FFeedEntry& Entry) { return Entry.Age > 4.f; });

	if (HitStopLeft > 0.f)
	{
		HitStopLeft -= RealDelta;
		if (HitStopLeft <= 0.f && !bPlayerDead)
		{
			UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
		}
	}

	switch (Flow)
	{
	case EMuseumFlow::Menu:
		UpdateMenuCamera(RealDelta);
		break;

	case EMuseumFlow::Playing:
	case EMuseumFlow::Intermission:
	{
		if (bPlayerDead)
		{
			DeadTimer -= RealDelta;
			if (DeadTimer <= 0.f) { EndRun(false); }
			break;
		}

		RunTime += DeltaTime;
		ComboTimeLeft = FMath::Max(ComboTimeLeft - DeltaTime, 0.f);
		if (ComboTimeLeft <= 0.f) { Combo = 0; }

		// The night goes on: dusk at the first wave, dawn after the last one
		const float WaveProgress = WaveTotal > 0 ? 1.f - float(GetEnemiesLeft()) / WaveTotal : 1.f;
		const float Fraction = (float(Wave - 1) + (Flow == EMuseumFlow::Intermission ? 1.f : WaveProgress)) / Waves.Num();
		const float TargetHour = DayBreaks ? 31.3f : FMath::Lerp(17.6f, 29.2f, Fraction);
		if (!bHourLocked)
		{
			SmoothHour = FMath::FInterpTo(SmoothHour, TargetHour, DeltaTime, DayBreaks ? 0.5f : 0.4f);
			ApplyHour(SmoothHour);
		}

		if (Flow == EMuseumFlow::Playing)
		{
			Alive.RemoveAll([](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid(); });
			SpawnTimer -= DeltaTime;
			if (Pending.Num() > 0 && Alive.Num() < GetWaveDef().MaxAlive && SpawnTimer <= 0.f)
			{
				SpawnNext();
			}

			// Safety net: somebody got stuck behind a door. After a while the last enemies are brought back into the game
			TimeSinceKill += DeltaTime;
			if (Pending.Num() == 0 && Alive.Num() > 0 && Alive.Num() <= 3 && TimeSinceKill > 45.f)
			{
				TimeSinceKill = 20.f;
				for (const TWeakObjectPtr<AActor>& Entry : Alive)
				{
					FVector Spot;
					if (Entry.IsValid() && FindSpawnLocation(Spot))
					{
						Entry->SetActorLocation(Spot + FVector(0.f, 0.f, 120.f), false, nullptr, ETeleportType::TeleportPhysics);
					}
				}
			}

			if (Pending.Num() == 0 && Alive.Num() == 0 && WaveTotal > 0)
			{
				WaveCleared();
			}
		}
		else if (!bPerkShown)
		{
			if (PerkDelay > 0.f)
			{
				PerkDelay -= RealDelta;
				if (PerkDelay <= 0.f)
				{
					if (Wave >= Waves.Num()) { EndRun(true); }
					else { ShowPerkChoice(); }
				}
			}
			else if (IntermissionLeft > 0.f)
			{
				IntermissionLeft -= DeltaTime;
				if (IntermissionLeft <= 0.f) { FinishIntermission(); }
			}
		}
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Debug / console commands (mg.*), compiled out of shipping builds
// ---------------------------------------------------------------------------------------------------------------

void UMuseumDirector::DebugStartWave(int32 WaveNumber)
{
	if (Flow == EMuseumFlow::Menu || Flow == EMuseumFlow::GameOver || Flow == EMuseumFlow::Victory) { BeginRun(); }
	DestroyAllEnemies();
	DayBreaks = false;
	BeginWave(WaveNumber);
}

void UMuseumDirector::DebugKillAll()
{
	TArray<AActor*> Actors;
	TArray<AActor*> Guards;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemy::StaticClass(), Actors);
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGuardNPC::StaticClass(), Guards);
	Actors.Append(Guards);
	for (AActor* Actor : Actors)
	{
		UGameplayStatics::ApplyDamage(Actor, 1000000.f, GetPC(), GetPlayer(), UDamageType::StaticClass());
	}
	Pending.Reset();
}

void UMuseumDirector::DebugSetPage(EMuseumPage Page)
{
	if (!UI) { return; }
	if (Page == EMuseumPage::Perks && Offers.Num() == 0)
	{
		Wave = FMath::Max(Wave, 1);
		bPerkShown = true;
		SetFlow(EMuseumFlow::Intermission);
		ShowPerkChoice();
		return;
	}
	UI->ShowPage(Page);
	SetMenuMode(Page != EMuseumPage::None);
}

void UMuseumDirector::DebugCamera(const FVector& Location, const FRotator& Rotation, float Fov)
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (!MenuCamera)
	{
		MenuCamera = World->SpawnActor<ACameraActor>(Location, Rotation);
		if (!MenuCamera) { return; }
		MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	bFreeCamera = true;
	MenuCamera->SetActorLocationAndRotation(Location, Rotation);
	MenuCamera->GetCameraComponent()->SetFieldOfView(Fov);
	if (APlayerController* PC = GetPC()) { PC->SetViewTargetWithBlend(MenuCamera, 0.f); }
}

void UMuseumDirector::DebugSpawn(EEnemyKind Kind, const FVector& Location, float Yaw, bool bFrozen)
{
	// Stand on whatever floor is under the given point (frozen enemies cannot fall)
	FVector Floor = Location;
	FHitResult Hit;
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 600.f), Location - FVector(0.f, 0.f, 1500.f), ECC_Visibility))
	{
		Floor = Hit.ImpactPoint;
	}
	if (AActor* Actor = SpawnEnemyAt(Kind, Floor, FRotator(0.f, Yaw, 0.f), false))
	{
		if (bFrozen) { Actor->CustomTimeDilation = 0.f; }
		// Staged enemies are for pictures: no health bars
		if (UWidgetComponent* Bar = Actor->FindComponentByClass<UWidgetComponent>()) { Bar->SetVisibility(false); }
	}
}

void UMuseumDirector::DebugHour(float Hour)
{
	if (DayNight)
	{
		DayNight->SetExternalControl(true);
		DayNight->SetHour(Hour);
		SmoothHour = Hour;
		bHourLocked = true;
	}
}

void UMuseumDirector::DebugUI(bool bVisible)
{
	bUiHidden = !bVisible;
	if (UI) { UI->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
}

void UMuseumDirector::DebugFire(int32 Count, float Yaw, float Pitch)
{
	if (APlayerController* PC = GetPC()) { PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f)); }
	DebugShotsLeft = Count;
	DebugShotTimer = 0.f;
}

void UMuseumDirector::DebugClear()
{
	DestroyAllEnemies();
	WaveTotal = 0;
	SpawnTimer = 1.0e9f;
}

void UMuseumDirector::DebugFreeze(bool bFreeze)
{
	UGameplayStatics::SetGamePaused(GetWorld(), bFreeze);
}

void UMuseumDirector::DebugGod(bool bOn)
{
	bGod = bOn;
	if (APlayerCharacter* Player = GetPlayer()) { Player->SetGodMode(bOn); }
}

void UMuseumDirector::DebugForceEnd(bool bVictory)
{
	Score = FMath::Max(Score, bVictory ? 38000 : 4200);
	Kills = FMath::Max(Kills, 61);
	Headshots = FMath::Max(Headshots, 17);
	RunTime = FMath::Max(RunTime, 612.f);
	Wave = bVictory ? Waves.Num() : FMath::Max(Wave, 5);
	EndRun(bVictory);
}

namespace
{
	TArray<TPair<FString, TArray<FString>>> GPendingCommands;
	float GCommandWait = 0.f;

	void ExecuteMuseumCommand(UMuseumDirector* D, const FString& Name, const TArray<FString>& Args)
	{
		const int32 Number = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		if (Name == TEXT("start")) { D->StartGame(); }
		else if (Name == TEXT("menu")) { D->ReturnToMenu(); }
		else if (Name == TEXT("retry")) { D->RestartRun(); }
		else if (Name == TEXT("wave")) { D->DebugStartWave(FMath::Max(Number, 1)); }
		else if (Name == TEXT("kill")) { D->DebugKillAll(); }
		else if (Name == TEXT("god")) { D->DebugGod(Number != 0); }
		else if (Name == TEXT("win")) { D->DebugForceEnd(true); }
		else if (Name == TEXT("lose")) { D->DebugForceEnd(false); }
		else if (Name == TEXT("cam"))
		{
			if (Args.Num() >= 5)
			{
				D->DebugCamera(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])),
					FRotator(FCString::Atof(*Args[3]), FCString::Atof(*Args[4]), 0.f), Args.Num() > 5 ? FCString::Atof(*Args[5]) : 70.f);
			}
			else { D->DebugSetMenuShot(Number); }
		}
		else if (Name == TEXT("spawn") && Args.Num() >= 4)
		{
			// mg.spawn <walker|runner|bronze|brute|guard|boss> x y z [yaw] [frozen 0/1]
			const FString Kind = Args[0].ToLower();
			const EEnemyKind Value = Kind == TEXT("runner") ? EEnemyKind::Runner : Kind == TEXT("bronze") ? EEnemyKind::Bronze : Kind == TEXT("brute") ? EEnemyKind::Brute
				: Kind == TEXT("guard") ? EEnemyKind::Guard : Kind == TEXT("boss") ? EEnemyKind::Boss : EEnemyKind::Walker;
			D->DebugSpawn(Value, FVector(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3])),
				Args.Num() > 4 ? FCString::Atof(*Args[4]) : 0.f, Args.Num() > 5 ? FCString::Atoi(*Args[5]) != 0 : true);
		}
		else if (Name == TEXT("quality"))
		{
			if (UMuseumUserSettings* S = UMuseumUserSettings::Get()) { S->SetOverallScalabilityLevel(FMath::Clamp(Number, 0, 3)); S->ApplyNonResolutionSettings(); }
		}
		else if (Name == TEXT("perk")) { D->ChoosePerk(Number); }
		else if (Name == TEXT("fire")) { D->DebugFire(FMath::Max(Number, 1), Args.Num() > 1 ? FCString::Atof(*Args[1]) : 180.f, Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f); }
		else if (Name == TEXT("clear")) { D->DebugClear(); }
		else if (Name == TEXT("hshot") && Args.Num() >= 2)
		{
			// mg.hshot 1920x1080 name: an off-screen high resolution shot without the UI
#if !UE_BUILD_SHIPPING
			const FString Path = FPaths::ProjectDir() / TEXT("Release/screens") / (Args[1] + TEXT(".png"));
			GEngine->Exec(D->GetWorld(), *FString::Printf(TEXT("HighResShot %s filename=\"%s\""), *Args[0], *Path));
			GCommandWait = 1.5f;
#endif
		}
		else if (Name == TEXT("ui")) { D->DebugUI(Number != 0); }
		else if (Name == TEXT("freeze")) { D->DebugFreeze(Number != 0); }
		else if (Name == TEXT("wait")) { GCommandWait = FCString::Atof(Args.Num() > 0 ? *Args[0] : TEXT("1")); }
		else if (Name == TEXT("hour")) { D->DebugHour(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 20.f); }
		else if (Name == TEXT("page"))
		{
			const FString Page = Args.Num() > 0 ? Args[0] : TEXT("");
			EMuseumPage Target = EMuseumPage::None;
			if (Page == TEXT("main")) { Target = EMuseumPage::Main; }
			else if (Page == TEXT("settings")) { Target = EMuseumPage::Settings; }
			else if (Page == TEXT("howto")) { Target = EMuseumPage::HowTo; }
			else if (Page == TEXT("pause")) { Target = EMuseumPage::Pause; }
			else if (Page == TEXT("perks")) { Target = EMuseumPage::Perks; }
			else if (Page == TEXT("gameover")) { Target = EMuseumPage::GameOver; }
			else if (Page == TEXT("victory")) { Target = EMuseumPage::Victory; }
			D->DebugSetPage(Target);
		}
		else if (Name == TEXT("shot"))
		{
#if !UE_BUILD_SHIPPING
			const FString File = Args.Num() > 0 ? Args[0] : TEXT("shot");
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir() / TEXT("Release/screens") / (File + TEXT(".png")), true, false);
			GCommandWait = 0.7f;
#endif
		}
	}

	// Always queued: the director runs them one per frame and honours mg.wait between them
	void QueueOrRun(UWorld* World, const FString& Name, const TArray<FString>& Args)
	{
		GPendingCommands.Add({ Name, Args });
	}

#define MG_COMMAND(CmdName) \
	static FAutoConsoleCommandWithWorldAndArgs GMgCmd_##CmdName(TEXT("mg." #CmdName), TEXT("Museum game test command"), \
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { QueueOrRun(World, TEXT(#CmdName), Args); }));

	MG_COMMAND(start) MG_COMMAND(retry) MG_COMMAND(menu) MG_COMMAND(wave) MG_COMMAND(kill) MG_COMMAND(god) MG_COMMAND(win)
	MG_COMMAND(lose) MG_COMMAND(cam) MG_COMMAND(wait) MG_COMMAND(page) MG_COMMAND(shot) MG_COMMAND(hour)
	MG_COMMAND(spawn) MG_COMMAND(ui) MG_COMMAND(freeze) MG_COMMAND(quality) MG_COMMAND(perk) MG_COMMAND(fire) MG_COMMAND(clear) MG_COMMAND(hshot)
}

void UMuseumDirector::HandleCommandQueue(float RealDelta)
{
	// Commands given on the command line (-ExecCmds="mg.start,mg.wait 3,mg.shot a") run once the level is up, one per frame
	if (StartupTime < 1.5f) { return; }
	if (GCommandWait > 0.f)
	{
		GCommandWait -= RealDelta;
		return;
	}
	if (GPendingCommands.Num() == 0) { return; }

	const TPair<FString, TArray<FString>> Command = GPendingCommands[0];
	GPendingCommands.RemoveAt(0);
	UE_LOG(LogTemp, Display, TEXT("mg command: %s %s"), *Command.Key, *FString::Join(Command.Value, TEXT(" ")));
	ExecuteMuseumCommand(this, Command.Key, Command.Value);
}
