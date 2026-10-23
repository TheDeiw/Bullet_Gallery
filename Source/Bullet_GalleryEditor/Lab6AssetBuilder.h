#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Lab6AssetBuilder.generated.h"

/**
 * Builds the Lab 6 assets in /Game/_MuseumProject/Lab6 that the editor tools cannot create:
 * - AM_Player_Reload: montage of MM_Pistol_Reload cut into ReloadStart -> ReloadLoop (loops on itself) -> ReloadEnd,
 *   with a "RoundsInserted" Montage Notify near the end of the loop (rebuilt on every request);
 * - ABP_Guard: Animation Blueprint with the parent UGuardAnimInstance (only created when missing);
 * - MPC_Weather: Material Parameter Collection WindStrength / WindSpeed / RainAmount (only created when missing).
 *
 * Same trigger as UGuardBehaviorTreeBuilder: set bBuildRequested on the class default object
 * (/Script/Bullet_GalleryEditor.Default__Lab6AssetBuilder); the editor module runs the build and writes LastResult.
 */
UCLASS()
class ULab6AssetBuilder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Build")
	bool bBuildRequested = false;

	/** Cut points in MM_Pistol_Reload, seconds: before LoopStartTime the pistol goes down, after LoopEndTime it comes up */
	UPROPERTY(EditAnywhere, Category = "Reload")
	float LoopStartTime = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Reload")
	float LoopEndTime = 1.25f;

	/** The "RoundsInserted" notify fires this long before the end of the loop section, seconds */
	UPROPERTY(EditAnywhere, Category = "Reload")
	float NotifyBeforeLoopEnd = 0.1f;

	UPROPERTY(VisibleAnywhere, Category = "Build")
	FString LastResult;

	/** Builds all three assets; returns a short report */
	FString BuildAll() const;
};
