#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HeavyHUD.generated.h"

class AHeavyCharacter;

/**
 * Canvas-drawn HUD, no widget assets:
 *  - A shotgun reticle drawn where the gun is actually pointing (it trails the screen centre
 *    when the weapon lags) sized to the real pellet spread, plus a faint dot at screen centre.
 *  - Hit markers, floating damage numbers and health bars on damaged targets.
 *  - Shell counter and reload state.
 *  - Controls and a small tuning readout (toggle with H).
 */
UCLASS()
class HEAVYSHOOTER_API AHeavyHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	FVector2D DrawReticle(AHeavyCharacter* Character);
	void DrawHitMarker(AHeavyCharacter* Character, const FVector2D& Reticle);
	void DrawDamageNumbers(AHeavyCharacter* Character);
	void DrawTargetBars(AHeavyCharacter* Character);
	void DrawAmmo(AHeavyCharacter* Character);
	void DrawHelp(AHeavyCharacter* Character);

	void DrawCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments = 40);
	void DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, class UFont* Font, float Scale, bool bCentered = false);
	bool ProjectToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const;
	float UIScale() const;
};
