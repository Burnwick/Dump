#include "HeavyPlayerController.h"

#include "Camera/PlayerCameraManager.h"

void AHeavyPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}

	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -75.f;
		PlayerCameraManager->ViewPitchMax = 70.f;
	}
}
