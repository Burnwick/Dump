#include "HeavyGameMode.h"

#include "HeavyCharacter.h"
#include "HeavyHUD.h"
#include "HeavyPlayerController.h"
#include "HeavyShooter.h"
#include "HeavyTestRange.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"

AHeavyGameMode::AHeavyGameMode()
{
	DefaultPawnClass = AHeavyCharacter::StaticClass();
	PlayerControllerClass = AHeavyPlayerController::StaticClass();
	HUDClass = AHeavyHUD::StaticClass();
}

AActor* AHeavyGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	if (FallbackStart)
	{
		return FallbackStart.Get();
	}

	AActor* Start = Super::FindPlayerStart_Implementation(Player, IncomingName);
	const bool bFoundRealStart = Start && !Start->IsA<AWorldSettings>();
	if (bFoundRealStart || !bAutoBuildTestRange)
	{
		return Start;
	}

	// No player start at all: put one where the test range expects the player.
	// The range itself is built in StartPlay.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FallbackStart = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(),
		AHeavyTestRange::GetPlayerStartOffset(), FRotator::ZeroRotator, Params);
	return FallbackStart ? FallbackStart.Get() : Start;
}

void AHeavyGameMode::StartPlay()
{
	if (bAutoBuildTestRange)
	{
		EnsureTestRange();
	}
	Super::StartPlay();
}

void AHeavyGameMode::EnsureTestRange()
{
	UWorld* World = GetWorld();
	if (!World || TActorIterator<AHeavyTestRange>(World))
	{
		return; // Already has one (placed by hand).
	}

	APlayerStart* LevelStart = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (*It != FallbackStart)
		{
			LevelStart = *It;
			break;
		}
	}

	FTransform RangeTransform = FTransform::Identity;
	if (LevelStart)
	{
		// A real level with ground under its start is left alone.
		const FVector From = LevelStart->GetActorLocation();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(HeavyFindFloor), false, LevelStart);
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, From, From - FVector(0.f, 0.f, 20000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Query))
		{
			return;
		}

		// A start floating over nothing (e.g. the blank Entry map): build the range under it.
		const FRotator Facing(0.f, LevelStart->GetActorRotation().Yaw, 0.f);
		RangeTransform = FTransform(Facing, From - Facing.RotateVector(AHeavyTestRange::GetPlayerStartOffset()));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (World->SpawnActor<AHeavyTestRange>(AHeavyTestRange::StaticClass(), RangeTransform, Params))
	{
		UE_LOG(LogHeavy, Log, TEXT("Empty level detected: built the heavy test range."));
	}
}
