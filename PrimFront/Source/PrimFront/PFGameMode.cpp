#include "PFGameMode.h"
#include "PFCharacter.h"
#include "PFHUD.h"
#include "PFRangeBuilder.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

APFGameMode::APFGameMode()
{
	DefaultPawnClass = APFCharacter::StaticClass();
	HUDClass = APFHUD::StaticClass();
}

void APFGameMode::StartPlay()
{
	EnsureRange();
	Super::StartPlay();
}

void APFGameMode::RestartPlayer(AController* NewPlayer)
{
	// Players can log in before StartPlay, so make sure the floor exists before spawning them
	EnsureRange();

	bool bHasPlayerStart = false;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		bHasPlayerStart = true;
		break;
	}

	// The default (empty) map has no PlayerStart: spawn behind the range's firing line instead
	if (NewPlayer && !bHasPlayerStart)
	{
		RestartPlayerAtTransform(NewPlayer, APFRangeBuilder::GetPlayerSpawnTransform());
		return;
	}

	Super::RestartPlayer(NewPlayer);
}

void APFGameMode::EnsureRange()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APFRangeBuilder> It(World); It; ++It)
	{
		It->Build();
		return;
	}

	if (!bAutoBuildRange)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APFRangeBuilder* Range = World->SpawnActor<APFRangeBuilder>(APFRangeBuilder::StaticClass(), FTransform::Identity, Params))
	{
		Range->Build();
	}
}
