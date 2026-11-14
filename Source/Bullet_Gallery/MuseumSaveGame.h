#pragma once

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/SaveGame.h"
#include "MuseumSaveGame.generated.h"

/** Records kept between runs (slot "MuseumProgress"): the reason to try one more time */
UCLASS()
class BULLET_GALLERY_API UMuseumSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 BestScore = 0;
	UPROPERTY() int32 BestWave = 0;
	UPROPERTY() int32 TotalKills = 0;
	UPROPERTY() int32 GamesPlayed = 0;
	UPROPERTY() int32 Victories = 0;
	/** Fastest victory, seconds; 0 = none yet */
	UPROPERTY() float BestVictoryTime = 0.f;

	static constexpr const TCHAR* SlotName = TEXT("MuseumProgress");

	static UMuseumSaveGame* Load()
	{
		UMuseumSaveGame* Save = UGameplayStatics::DoesSaveGameExist(SlotName, 0) ? Cast<UMuseumSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)) : nullptr;
		return Save ? Save : Cast<UMuseumSaveGame>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
	}

	void Write() { UGameplayStatics::SaveGameToSlot(this, SlotName, 0); }
};
