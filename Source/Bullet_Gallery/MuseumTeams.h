#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "MuseumTeams.generated.h"

/** Teams of the museum. The AI perception asks a character for its team to decide if it is an enemy */
enum EMuseumTeam : uint8
{
	PlayersTeam = 1,
	GuardsTeam = 2
};

/** Team of the player (the "FPlayerTeamID" of the lab guide) */
USTRUCT()
struct FPlayerTeamID : public FGenericTeamId
{
	GENERATED_BODY()

	FPlayerTeamID() : FGenericTeamId(PlayersTeam) {}
};

/** Team of the museum guards */
USTRUCT()
struct FGuardTeamID : public FGenericTeamId
{
	GENERATED_BODY()

	FGuardTeamID() : FGenericTeamId(GuardsTeam) {}
};
