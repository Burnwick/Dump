#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HeavyPlayerController.generated.h"

UCLASS()
class HEAVYSHOOTER_API AHeavyPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
