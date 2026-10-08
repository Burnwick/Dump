#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HeavyGameMode.generated.h"

class APlayerStart;

/**
 * Sets up the heavy character, HUD and controller. If the level is empty (no player start, or a
 * player start with nothing underneath it) it builds an AHeavyTestRange so the prototype is
 * playable straight from the engine's blank map.
 */
UCLASS()
class HEAVYSHOOTER_API AHeavyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHeavyGameMode();

	virtual void StartPlay() override;
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy")
	bool bAutoBuildTestRange = true;

private:
	void EnsureTestRange();

	UPROPERTY(Transient)
	TObjectPtr<APlayerStart> FallbackStart;
};
