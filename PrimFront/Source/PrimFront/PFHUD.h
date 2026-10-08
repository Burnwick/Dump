#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PFHUD.generated.h"

class APFCharacter;
class UFont;

/** Canvas-only HUD (no widget assets): shotgun spread circle, hit markers, ammo, stance, range stats and controls. */
UCLASS()
class PRIMFRONT_API APFHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCrosshair(const APFCharacter* Char, float CX, float CY, float S);
	void DrawHitMarker(const APFCharacter* Char, float CX, float CY, float S);
	void DrawAmmo(const APFCharacter* Char, float S);
	void DrawStatus(const APFCharacter* Char, float S);
	void DrawStats(const APFCharacter* Char, float S);
	void DrawHelp(float S);
	void DrawShadowedText(const FString& Text, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale);
};
