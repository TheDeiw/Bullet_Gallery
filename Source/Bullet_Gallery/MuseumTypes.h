#pragma once

#include "CoreMinimal.h"
#include "MuseumTypes.generated.h"

/** The kinds of enemies the wave director can bring into the museum */
UENUM(BlueprintType)
enum class EEnemyKind : uint8
{
	Walker,   // marble statue, the basic enemy
	Runner,   // light and fast
	Bronze,   // tough bronze statue
	Brute,    // huge, slow, hits hard
	Guard,    // night guard with a revolver (AI behaviour tree)
	Boss      // the Curator: the last wave
};

/** Perks the player picks between the waves */
UENUM(BlueprintType)
enum class EMuseumPerk : uint8
{
	Damage,
	FireRate,
	Reload,
	Ammo,
	Health,
	Speed,
	Leech,
	Scavenger,
	Count UMETA(Hidden)
};

/** How an enemy differs from its Blueprint defaults; filled by the director for every spawned enemy */
struct FEnemyVariant
{
	EEnemyKind Kind = EEnemyKind::Walker;
	FText Name;
	float HealthMul = 1.f;
	float SpeedMul = 1.f;
	float DamageMul = 1.f;
	float Scale = 1.f;
	/** Score for the kill before the combo multiplier */
	int32 Points = 100;
	/** Glow of the eyes / body; alpha 0 = none */
	FLinearColor Aura = FLinearColor(0.f, 0.f, 0.f, 0.f);
	/** Knows where the player is from the moment it appears (wave enemies hunt the player) */
	bool bHunter = true;
};
