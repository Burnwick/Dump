#include "PFHUD.h"
#include "PFCharacter.h"
#include "PFShotgun.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const FLinearColor HudWhite(1.f, 1.f, 1.f, 0.9f);
	const FLinearColor HudDim(1.f, 1.f, 1.f, 0.45f);
	const FLinearColor HudAmber(1.f, 0.75f, 0.15f, 1.f);
	const FLinearColor HudRed(1.f, 0.22f, 0.15f, 1.f);
	const FLinearColor HudShellEmpty(0.15f, 0.15f, 0.15f, 0.7f);
	const FLinearColor HudPanel(0.f, 0.f, 0.f, 0.35f);
	constexpr float HudHitMarkerDuration = 0.25f;
}

void APFHUD::DrawHUD()
{
	Super::DrawHUD();

	const APFCharacter* Char = Cast<APFCharacter>(GetOwningPawn());
	if (!Canvas || !Char)
	{
		return;
	}

	const float S = Canvas->ClipY / 1080.f;
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	DrawCrosshair(Char, CX, CY, S);
	DrawHitMarker(Char, CX, CY, S);
	DrawAmmo(Char, S);
	DrawStatus(Char, S);
	DrawStats(Char, S);
	if (Char->ShouldShowHelp())
	{
		DrawHelp(S);
	}
}

void APFHUD::DrawShadowedText(const FString& Text, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale)
{
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + 2.f, Y + 2.f, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
}

void APFHUD::DrawCrosshair(const APFCharacter* Char, float CX, float CY, float S)
{
	// Holstered or sprinting: just a small dot
	if (!Char->IsWeaponOut() || Char->IsSprinting())
	{
		DrawRect(HudDim, CX - 2.f * S, CY - 2.f * S, 4.f * S, 4.f * S);
		return;
	}

	const bool bReady = !Char->IsReloading() && !Char->IsMeleeing();
	const FLinearColor Color = bReady ? HudWhite : HudDim;

	// Circle showing the pellet cone: project the spread angle onto the screen
	const float HalfFov = FMath::DegreesToRadians(Char->GetCameraFOV() * 0.5f);
	const float Spread = FMath::DegreesToRadians(Char->GetSpreadDegrees());
	const float Radius = FMath::Max(FMath::Tan(Spread) / FMath::Tan(HalfFov) * Canvas->ClipX * 0.5f, 6.f * S);

	constexpr int32 Segments = 40;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = 2.f * UE_PI * i / Segments;
		const float A1 = 2.f * UE_PI * (i + 1) / Segments;
		DrawLine(CX + FMath::Cos(A0) * Radius, CY + FMath::Sin(A0) * Radius,
			CX + FMath::Cos(A1) * Radius, CY + FMath::Sin(A1) * Radius, Color, 1.5f * S);
	}

	// Four ticks outside the circle and a centre dot
	const float Gap = Radius + 4.f * S;
	const float Tick = 10.f * S;
	DrawLine(CX + Gap, CY, CX + Gap + Tick, CY, Color, 2.f * S);
	DrawLine(CX - Gap, CY, CX - Gap - Tick, CY, Color, 2.f * S);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Tick, Color, 2.f * S);
	DrawLine(CX, CY - Gap, CX, CY - Gap - Tick, Color, 2.f * S);
	DrawRect(Color, CX - 1.5f * S, CY - 1.5f * S, 3.f * S, 3.f * S);
}

void APFHUD::DrawHitMarker(const APFCharacter* Char, float CX, float CY, float S)
{
	if (Char->HitMarkerTime <= 0.f)
	{
		return;
	}

	FLinearColor Color = Char->bHitMarkerKill ? HudRed : (Char->bHitMarkerHead ? HudAmber : HudWhite);
	Color.A = FMath::Clamp(Char->HitMarkerTime / HudHitMarkerDuration, 0.f, 1.f);

	const float Inner = 8.f * S;
	const float Outer = 20.f * S;
	for (const float DX : { -1.f, 1.f })
	{
		for (const float DY : { -1.f, 1.f })
		{
			DrawLine(CX + DX * Inner, CY + DY * Inner, CX + DX * Outer, CY + DY * Outer, Color, 2.5f * S);
		}
	}
}

void APFHUD::DrawAmmo(const APFCharacter* Char, float S)
{
	const APFShotgun* Weapon = Char->GetWeapon();
	if (!Weapon)
	{
		return;
	}

	UFont* Large = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();

	const float Width = 300.f * S;
	const float X = Canvas->ClipX - Width - 40.f * S;
	const float Y = Canvas->ClipY - 170.f * S;
	DrawRect(HudPanel, X - 16.f * S, Y - 12.f * S, Width + 32.f * S, 150.f * S);

	const int32 Ammo = Weapon->GetAmmo();
	const FLinearColor AmmoColor = Ammo == 0 ? HudRed : (Ammo <= 2 ? HudAmber : HudWhite);
	DrawShadowedText(FString::Printf(TEXT("%d"), Ammo), X, Y, AmmoColor, Large, 2.2f * S);
	DrawShadowedText(FString::Printf(TEXT("/ %d"), Weapon->GetReserve()), X + 80.f * S, Y + 22.f * S, HudDim, Large, 1.3f * S);
	DrawShadowedText(TEXT("PUMP SHOTGUN"), X + 170.f * S, Y + 6.f * S, HudDim, Small, 1.1f * S);

	// One block per shell in the tube
	const float ShellW = 11.f * S;
	const float ShellH = 24.f * S;
	for (int32 i = 0; i < Weapon->GetMagSize(); ++i)
	{
		DrawRect(i < Ammo ? HudAmber : HudShellEmpty, X + i * (ShellW + 5.f * S), Y + 70.f * S, ShellW, ShellH);
	}

	FString Hint;
	FLinearColor HintColor = HudAmber;
	if (Char->IsReloading())
	{
		Hint = TEXT("RELOADING  (fire to stop)");
	}
	else if (Char->IsWeaponHolstered())
	{
		Hint = TEXT("HOLSTERED  (H / fire to draw)");
		HintColor = HudDim;
	}
	else if (Ammo == 0 && Weapon->GetReserve() == 0)
	{
		Hint = TEXT("OUT OF AMMO  (T to refill)");
		HintColor = HudRed;
	}
	else if (Ammo < Weapon->GetMagSize() && Ammo <= 2)
	{
		Hint = TEXT("R  RELOAD");
	}
	if (!Hint.IsEmpty())
	{
		DrawShadowedText(Hint, X, Y + 104.f * S, HintColor, Small, 1.2f * S);
	}
}

void APFHUD::DrawStatus(const APFCharacter* Char, float S)
{
	UFont* Font = GEngine->GetSmallFont();
	const float X = 40.f * S;
	float Y = Canvas->ClipY - 150.f * S;
	const float Line = 26.f * S;

	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	FString Stance = Char->bIsCrouched ? TEXT("CROUCHED") : TEXT("STANDING");
	if (Move && Move->IsFalling())
	{
		Stance = TEXT("AIRBORNE");
	}
	else if (Char->IsSprinting())
	{
		Stance = TEXT("SPRINTING");
	}

	DrawRect(HudPanel, X - 16.f * S, Y - 10.f * S, 300.f * S, 4.f * Line + 16.f * S);
	DrawShadowedText(Stance, X, Y, HudWhite, Font, 1.3f * S);
	Y += Line;
	DrawShadowedText(Char->GetAimAlpha() > 0.5f ? TEXT("AIMING DOWN SIGHTS") : TEXT("HIP FIRE"), X, Y, Char->GetAimAlpha() > 0.5f ? HudAmber : HudDim, Font, 1.2f * S);
	Y += Line;
	DrawShadowedText(Char->IsShoulderRight() ? TEXT("SHOULDER: RIGHT") : TEXT("SHOULDER: LEFT"), X, Y, HudDim, Font, 1.2f * S);
	Y += Line;
	DrawShadowedText(Char->IsWeaponHolstered() ? TEXT("WEAPON: HOLSTERED") : TEXT("WEAPON: DRAWN"), X, Y, HudDim, Font, 1.2f * S);
}

void APFHUD::DrawStats(const APFCharacter* Char, float S)
{
	UFont* Font = GEngine->GetSmallFont();
	const float Width = 300.f * S;
	const float X = Canvas->ClipX - Width - 40.f * S;
	float Y = 40.f * S;
	const float Line = 26.f * S;

	const int32 Accuracy = Char->PelletsFired > 0 ? FMath::RoundToInt(100.f * Char->PelletsHit / Char->PelletsFired) : 0;
	const FString Lines[] = {
		FString::Printf(TEXT("SHOTS FIRED       %d"), Char->ShotsFired),
		FString::Printf(TEXT("PELLET ACCURACY   %d%%"), Accuracy),
		FString::Printf(TEXT("TARGETS DOWN      %d"), Char->Kills),
		FString::Printf(TEXT("HEADSHOTS         %d"), Char->Headshots),
		FString::Printf(TEXT("MELEE HITS        %d"), Char->MeleeHits)
	};

	DrawRect(HudPanel, X - 16.f * S, Y - 10.f * S, Width + 32.f * S, (float)UE_ARRAY_COUNT(Lines) * Line + 16.f * S);
	for (const FString& Text : Lines)
	{
		DrawShadowedText(Text, X, Y, HudWhite, Font, 1.2f * S);
		Y += Line;
	}
}

void APFHUD::DrawHelp(float S)
{
	UFont* Font = GEngine->GetSmallFont();
	const float X = 40.f * S;
	float Y = 40.f * S;
	const float Line = 24.f * S;

	const TCHAR* Lines[] = {
		TEXT("WASD          move"),
		TEXT("Mouse         look"),
		TEXT("LMB           fire (pump shotgun)"),
		TEXT("RMB (hold)    aim down sights"),
		TEXT("Shift (hold)  sprint"),
		TEXT("C / Ctrl      crouch"),
		TEXT("Space         jump"),
		TEXT("R             reload (shell by shell)"),
		TEXT("Q             swap shoulder"),
		TEXT("H / 1         holster / draw"),
		TEXT("V / F         melee (butt stroke / punch)"),
		TEXT("T             reset targets + ammo"),
		TEXT("F1            hide this help")
	};

	DrawRect(HudPanel, X - 16.f * S, Y - 10.f * S, 420.f * S, (float)UE_ARRAY_COUNT(Lines) * Line + 16.f * S);
	for (const TCHAR* Text : Lines)
	{
		DrawShadowedText(Text, X, Y, HudWhite, Font, 1.1f * S);
		Y += Line;
	}
}
