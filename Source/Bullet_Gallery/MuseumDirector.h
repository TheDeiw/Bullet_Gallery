#pragma once

#include "CoreMinimal.h"
#include "MuseumTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MuseumDirector.generated.h"

class ACameraActor;
class ADayNightCycle;
class APickupBase;
class APlayerCharacter;
class APostProcessVolume;
class UAudioComponent;
class UMuseumUIWidget;
class UMuseumSaveGame;
class USoundBase;
class USoundClass;
class USoundMix;

/** What the game is doing right now */
UENUM()
enum class EMuseumFlow : uint8
{
	Menu,          // main menu over a slow camera flight, the world is frozen
	Playing,       // a wave is on
	Intermission,  // wave cleared: perk choice and a short rest
	Paused,
	GameOver,
	Victory
};

/** Screens of the menu widget */
UENUM()
enum class EMuseumPage : uint8
{
	None,
	Main,
	Settings,
	HowTo,
	Pause,
	Perks,
	GameOver,
	Victory
};

/** One enemy group of a wave */
struct FWaveEntry
{
	EEnemyKind Kind;
	int32 Count;
};

struct FWaveDef
{
	FText Title;
	FText Subtitle;
	TArray<FWaveEntry> Entries;
	/** How many enemies may be alive at the same time */
	int32 MaxAlive = 6;
	/** Pause between two spawns, seconds */
	float SpawnInterval = 1.5f;
};

/** One line of the small "+150 RUNNER" list next to the crosshair */
struct FFeedEntry
{
	FText Text;
	FLinearColor Color;
	float Age = 0.f;
};

/** A perk card */
struct FPerkInfo
{
	FText Name;
	FText Description;
	FLinearColor Color;
};

/**
 * The brain of the game loop. It lives in the museum level (created by the engine, no Blueprint or level change needed):
 * shows the main menu over a camera flight, starts a night of eight waves, spawns the enemies around the player, counts the
 * score and the combo, offers a perk between the waves, handles the pause, the game over and the victory at dawn.
 */
UCLASS()
class BULLET_GALLERY_API UMuseumDirector : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UMuseumDirector();

	static UMuseumDirector* Get(const UObject* WorldContext);

	// ---------- Subsystem ----------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

	// ---------- Commands from the UI and the input ----------
	void StartGame();
	void Resume();
	void RestartRun();
	void ReturnToMenu();
	void QuitGame();
	void ShowSettings();
	void ShowHowTo();
	/** Escape / Start button: back one level, or pause / resume */
	void OnBackPressed();
	void ChoosePerk(int32 Index);
	/** F key: the player's flashlight */
	void ToggleFlashlight();
	/** Saves the settings, applies them and goes back to where the settings were opened from */
	void CloseSettings();

	// ---------- Events from the game ----------
	void NotifyEnemyKilled(AActor* Enemy, const FEnemyVariant& Variant, bool bHeadshot);
	void NotifyPlayerDied();
	void NotifyPlayerHurt(float Damage);
	/** Hit marker sound (and the kill ding) for a bullet that hit an enemy */
	void PlayHitFeedback(bool bHeadshot, bool bKilled);
	/** Short slow motion on a big moment (boss kill, last kill) */
	void HitStop(float RealSeconds, float Dilation = 0.08f);

	// ---------- State for the UI ----------
	EMuseumFlow GetFlow() const { return Flow; }
	/** The world is running and the player is in control (crosshair and HUD are shown) */
	bool IsGameplayActive() const { return !bUiHidden && (Flow == EMuseumFlow::Playing || Flow == EMuseumFlow::Intermission); }
	int32 GetWave() const { return Wave; }
	int32 GetTotalWaves() const { return Waves.Num(); }
	int32 GetScore() const { return Score; }
	int32 GetKills() const { return Kills; }
	int32 GetHeadshots() const { return Headshots; }
	int32 GetBestScore() const;
	int32 GetMultiplier() const;
	int32 GetCombo() const { return Combo; }
	float GetComboFraction() const { return ComboTimeLeft / ComboWindow; }
	float GetRunTime() const { return RunTime; }
	int32 GetEnemiesLeft() const;
	int32 GetWaveEnemyTotal() const { return WaveTotal; }
	bool IsNewBest() const { return bNewBest; }
	float GetIntermissionTime() const { return IntermissionLeft; }
	const FWaveDef& GetWaveDef() const { return Waves[FMath::Clamp(Wave - 1, 0, Waves.Num() - 1)]; }
	const UMuseumSaveGame* GetSave() const { return Save; }
	FString GetRank() const;
	int32 GetPerkLevel(EMuseumPerk Perk) const { return PerkLevels.IsValidIndex((int32)Perk) ? PerkLevels[(int32)Perk] : 0; }
	const TArray<EMuseumPerk>& GetOffers() const { return Offers; }
	static const FPerkInfo& GetPerkInfo(EMuseumPerk Perk);

	/** Banner in the upper part of the screen; Age runs from 0 to BannerDuration */
	const FText& GetBannerTitle() const { return BannerTitle; }
	const FText& GetBannerSub() const { return BannerSub; }
	const FLinearColor& GetBannerColor() const { return BannerColor; }
	float GetBannerAge() const { return BannerAge; }
	float GetBannerDuration() const { return BannerDuration; }
	const TArray<FFeedEntry>& GetFeed() const { return Feed; }

	/** The boss that is alive, if any, with its health 0..1 */
	AActor* GetBoss() const { return Boss.Get(); }
	float GetBossHealth() const;

	void ShowBanner(const FText& Title, const FText& Sub, const FLinearColor& Color, float Duration = 2.6f);
	void PushFeed(const FText& Text, const FLinearColor& Color);

	/** Screen of the menu widget that is open now */
	EMuseumPage GetPage() const;

	// ---------- Testing helpers (console commands mg.*) ----------
	void DebugStartWave(int32 WaveNumber);
	void DebugKillAll();
	void DebugSetPage(EMuseumPage Page);
	void DebugGod(bool bOn);
	void DebugSetMenuShot(int32 Index);
	void DebugForceEnd(bool bVictory);
	/** Free camera for key-art screenshots */
	void DebugCamera(const FVector& Location, const FRotator& Rotation, float Fov);
	/** Places one enemy (frozen in its first pose when bFrozen) */
	void DebugSpawn(EEnemyKind Kind, const FVector& Location, float Yaw, bool bFrozen);
	void DebugHour(float Hour);
	void DebugUI(bool bVisible);
	void DebugFreeze(bool bFreeze);
	/** Removes every enemy and stops the wave (staging screenshots) */
	void DebugClear();
	/** Fires the pistol a number of times without the mouse (hit marker and damage number checks) */
	void DebugFire(int32 Count, float Yaw, float Pitch);

private:
	void BuildWaves();
	void BuildPerkInfos();
	/** Loads the Blueprints and sounds the game needs by path (they are cooked through DirectoriesToAlwaysCook) */
	void LoadGameAssets();
	void SetFlow(EMuseumFlow NewFlow);
	void EnterMenu();
	void BeginRun();
	void BeginWave(int32 WaveNumber);
	void WaveCleared();
	void FinishIntermission();
	void EndRun(bool bVictory);
	void ShowPerkChoice();

	/** Pauses or resumes the world and switches between the UI and the game input */
	void SetMenuMode(bool bMenuOpen);
	void ApplyAudio();
	void ApplyLook();
	void ApplyHour(float Hour);
	void AssignSoundClasses();
	void CacheFallbackSpawns();

	void SpawnNext();
	AActor* SpawnEnemy(EEnemyKind Kind);
	AActor* SpawnEnemyAt(EEnemyKind Kind, const FVector& FloorLocation, const FRotator& Rotation, bool bHunter);
	bool FindSpawnLocation(FVector& OutLocation) const;
	void DestroyAllEnemies();
	void DropLoot(const FVector& Location, EEnemyKind Kind);
	FEnemyVariant MakeVariant(EEnemyKind Kind) const;
	void UpdateMenuCamera(float RealDelta);
	void UpdateLook(float RealDelta);
	void HandleCommandQueue(float RealDelta);
	APlayerCharacter* GetPlayer() const;
	APlayerController* GetPC() const;
	void PlayUISound(bool bClick);
	void SaveProgress(bool bVictory);

	// ---------- Game state ----------
	EMuseumFlow Flow = EMuseumFlow::Menu;
	EMuseumFlow FlowBeforeSettings = EMuseumFlow::Menu;
	int32 Wave = 0;
	int32 Score = 0;
	int32 Kills = 0;
	int32 Headshots = 0;
	int32 Combo = 0;
	float ComboTimeLeft = 0.f;
	float RunTime = 0.f;
	float DamageTaken = 0.f;
	bool bNewBest = false;
	bool bPlayerDead = false;
	float DeadTimer = 0.f;
	bool bRunStarted = false;
	bool bGod = false;

	TArray<FWaveDef> Waves;
	TArray<EEnemyKind> Pending;
	TArray<TWeakObjectPtr<AActor>> Alive;
	TWeakObjectPtr<AActor> Boss;
	int32 WaveTotal = 0;
	float SpawnTimer = 0.f;
	float IntermissionLeft = 0.f;
	bool bPerkShown = false;
	float PerkDelay = 0.f;
	TArray<EMuseumPerk> Offers;
	TArray<int32> PerkLevels;

	static constexpr float ComboWindow = 5.f;

	FText BannerTitle;
	FText BannerSub;
	FLinearColor BannerColor = FLinearColor::White;
	float BannerAge = 100.f;
	float BannerDuration = 2.6f;
	TArray<FFeedEntry> Feed;

	// ---------- Hit stop ----------
	float HitStopLeft = 0.f;

	// ---------- Test shots ----------
	int32 DebugShotsLeft = 0;
	/** Seconds until the first-run hint is shown (0 = none) */
	float HintTimer = 0.f;
	float DebugShotTimer = 0.f;

	// ---------- Menu camera ----------
	struct FMenuShot
	{
		FVector From;
		FVector To;
		FVector LookAt;
		float Duration;
		float Fov;
	};
	TArray<FMenuShot> MenuShots;
	bool bFreeCamera = false;
	int32 MenuShotIndex = 0;
	float MenuShotTime = 0.f;
	FTimerHandle DummyHandle;

	UPROPERTY(Transient) TObjectPtr<ACameraActor> MenuCamera;
	UPROPERTY(Transient) TObjectPtr<UMuseumUIWidget> UI;
	UPROPERTY(Transient) TObjectPtr<UMuseumSaveGame> Save;
	UPROPERTY(Transient) TObjectPtr<APostProcessVolume> LookVolume;
	UPROPERTY(Transient) TObjectPtr<ADayNightCycle> DayNight;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicComponent;
	UPROPERTY(Transient) TObjectPtr<USoundMix> Mix;
	UPROPERTY() TObjectPtr<USoundClass> MasterClass;
	UPROPERTY() TObjectPtr<USoundClass> MusicClass;
	UPROPERTY() TObjectPtr<USoundClass> SfxClass;
	/** Hard references taken in the constructor so that the cooker packs the engine's sound classes with the game */
	UPROPERTY() TArray<TObjectPtr<USoundClass>> CookedSoundClasses;

	// ---------- Assets (found by path so that no Blueprint has to reference them) ----------
	UPROPERTY() TSubclassOf<AActor> WalkerClass;
	UPROPERTY() TSubclassOf<AActor> BronzeClass;
	UPROPERTY() TSubclassOf<AActor> GuardClass;
	UPROPERTY() TSubclassOf<AActor> HealthPickupClass;
	UPROPERTY() TSubclassOf<AActor> AmmoPickupClass;
	UPROPERTY() TObjectPtr<USoundBase> MusicSound;
	UPROPERTY() TObjectPtr<USoundBase> ClickSound;
	UPROPERTY() TObjectPtr<USoundBase> HoverSound;
	UPROPERTY() TObjectPtr<USoundBase> WaveStartSound;
	UPROPERTY() TObjectPtr<USoundBase> WaveClearSound;
	UPROPERTY() TObjectPtr<USoundBase> PerkSound;
	UPROPERTY() TObjectPtr<USoundBase> GameOverSound;
	UPROPERTY() TObjectPtr<USoundBase> VictorySound;
	UPROPERTY() TObjectPtr<USoundBase> HitMarkerSound;
	UPROPERTY() TObjectPtr<USoundBase> KillSound;

	TArray<FVector> FallbackSpawns;
	FVector PlayerStartLocation = FVector::ZeroVector;
	FRotator PlayerStartRotation = FRotator::ZeroRotator;

	/** Look settings applied to the post process volume; kept to blend the low-health effect */
	float LowHealthBlend = 0.f;
	float HurtPulse = 0.f;

	float SmoothHour = 18.4f;
	float WaveDamageTaken = 0.f;
	float TimeSinceKill = 0.f;
	/** The last wave is won: the sun rises */
	bool DayBreaks = false;
	/** mg.hour fixes the time of day (staging screenshots) */
	bool bHourLocked = false;
	/** mg.ui 0 hides every piece of the interface (staging screenshots) */
	bool bUiHidden = false;

	double LastRealTime = 0.0;
	float StartupTime = 0.f;
	bool bStartupDone = false;
};
