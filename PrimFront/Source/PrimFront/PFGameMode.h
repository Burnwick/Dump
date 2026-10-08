#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PFGameMode.generated.h"

/** Spawns the primitive shooting range (if the level doesn't have one) and the shotgun trooper. */
UCLASS()
class PRIMFRONT_API APFGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APFGameMode();

	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** Build the range automatically when the level has no APFRangeBuilder in it. */
	UPROPERTY(EditAnywhere, Category = "Range")
	bool bAutoBuildRange = true;

private:
	void EnsureRange();
};
