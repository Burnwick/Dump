#include "HeavyHUD.h"

#include "HeavyCharacter.h"
#include "HeavyShotgun.h"
#include "HeavyTargetDummy.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** FVector2D is double-precision; the canvas API takes floats. */
	void DrawSegment(AHUD* Hud, const FVector2D& From, const FVector2D& To, const FLinearColor& Color, float Thickness)
	{
		Hud->DrawLine(static_cast<float>(From.X), static_cast<float>(From.Y), static_cast<float>(To.X), static_cast<float>(To.Y), Color, Thickness);
	}

	void DrawDot(AHUD* Hud, const FVector2D& Center, float Size, const FLinearColor& Color)
	{
		Hud->DrawRect(Color, static_cast<float>(Center.X) - Size * 0.5f, static_cast<float>(Center.Y) - Size * 0.5f, Size, Size);
	}
}

namespace HudColors
{
	const FLinearColor Reticle(1.f, 1.f, 1.f, 0.9f);
	const FLinearColor OnTarget(1.f, 0.38f, 0.28f, 0.95f);
	const FLinearColor Kill(1.f, 0.2f, 0.15f, 1.f);
	const FLinearColor Critical(1.f, 0.85f, 0.25f, 1.f);
	const FLinearColor Text(0.92f, 0.92f, 0.88f, 1.f);
	const FLinearColor Dim(0.7f, 0.7f, 0.68f, 0.75f);
	const FLinearColor ShellHull(0.75f, 0.12f, 0.08f, 1.f);
	const FLinearColor ShellBrass(0.85f, 0.65f, 0.28f, 1.f);
	const FLinearColor ShellEmpty(0.15f, 0.15f, 0.15f, 0.45f);
	const FLinearColor Panel(0.f, 0.f, 0.f, 0.35f);
	const FLinearColor HealthFill(0.85f, 0.3f, 0.2f, 0.9f);
}

void AHeavyHUD::DrawHUD()
{
	Super::DrawHUD();

	AHeavyCharacter* Character = Cast<AHeavyCharacter>(GetOwningPawn());
	if (!Character || !Canvas)
	{
		return;
	}

	DrawTargetBars(Character);
	DrawDamageNumbers(Character);
	const FVector2D Reticle = DrawReticle(Character);
	DrawHitMarker(Character, Reticle);
	DrawAmmo(Character);
	DrawHelp(Character);
}

float AHeavyHUD::UIScale() const
{
	return FMath::Max(Canvas->ClipY / 1080.f, 0.5f);
}

bool AHeavyHUD::ProjectToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const
{
	const FVector Projected = Canvas->Project(WorldLocation);
	if (Projected.Z <= 0.0)
	{
		return false; // Behind the camera.
	}
	OutScreen = FVector2D(Projected.X, Projected.Y);
	return true;
}

void AHeavyHUD::DrawCircle(const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
{
	const float Step = 2.f * PI / static_cast<float>(Segments);
	FVector2D Previous = Center + FVector2D(Radius, 0.f);
	for (int32 Index = 1; Index <= Segments; ++Index)
	{
		const float Angle = Step * static_cast<float>(Index);
		const FVector2D Next = Center + FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);
		DrawSegment(this, Previous, Next, Color, Thickness);
		Previous = Next;
	}
}

void AHeavyHUD::DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale, bool bCentered)
{
	if (bCentered)
	{
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Text, Width, Height, Font, Scale);
		X -= Width * 0.5f;
		Y -= Height * 0.5f;
	}
	const float Offset = FMath::Max(1.f, Scale);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + Offset, Y + Offset, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
}

FVector2D AHeavyHUD::DrawReticle(AHeavyCharacter* Character)
{
	const FVector2D ScreenCenter(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// Draw the reticle where the gun is really pointing, not at the screen centre.
	FVector2D Reticle = ScreenCenter;
	ProjectToScreen(Character->GetAimPoint(), Reticle);

	// Size the circle to the actual pellet spread.
	float FOV = 90.f;
	if (PlayerOwner && PlayerOwner->PlayerCameraManager)
	{
		FOV = PlayerOwner->PlayerCameraManager->GetFOVAngle();
	}
	const float HalfFovTan = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(FOV, 10.f, 170.f) * 0.5f));
	const float SpreadTan = FMath::Tan(FMath::DegreesToRadians(Character->GetCurrentSpread()));
	const float Radius = FMath::Max(Canvas->ClipX * 0.5f * SpreadTan / HalfFovTan, 6.f);

	// Fade out while the gun is down for sprinting.
	const float Alpha = Character->IsSprinting() ? 0.2f : 1.f;
	FLinearColor Color = Character->IsAimPointOnTarget() ? HudColors::OnTarget : HudColors::Reticle;
	Color.A *= Alpha;

	const float Scale = UIScale();
	const float Thickness = 1.5f * Scale;
	DrawCircle(Reticle, Radius, Color, Thickness);

	const FVector2D Directions[] = { FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f) };
	for (const FVector2D& Direction : Directions)
	{
		const FVector2D Inner = Reticle + Direction * (Radius + 4.f * Scale);
		const FVector2D Outer = Reticle + Direction * (Radius + 12.f * Scale);
		DrawSegment(this, Inner, Outer, Color, Thickness);
	}
	DrawDot(this, Reticle, 3.f * Scale, Color);

	// A faint dot where the camera is looking, so you can see the gun trail behind it.
	const float LagPixels = static_cast<float>((Reticle - ScreenCenter).Size());
	if (LagPixels > 2.f)
	{
		const FLinearColor CenterColor(1.f, 1.f, 1.f, 0.35f * Alpha);
		DrawDot(this, ScreenCenter, 3.f * Scale, CenterColor);
		if (Character->ShouldShowHelp())
		{
			DrawSegment(this, ScreenCenter, Reticle, FLinearColor(1.f, 1.f, 1.f, 0.12f * Alpha), 1.f);
		}
	}

	return Reticle;
}

void AHeavyHUD::DrawHitMarker(AHeavyCharacter* Character, const FVector2D& Reticle)
{
	constexpr float Duration = 0.28f;
	const float Age = Character->GetGameTime() - Character->GetLastHitTime();
	if (Age < 0.f || Age > Duration)
	{
		return;
	}

	const float Fade = 1.f - Age / Duration;
	const float Scale = UIScale();
	FLinearColor Color = Character->WasLastHitKill() ? HudColors::Kill : FLinearColor::White;
	Color.A = Fade;

	const float Gap = 7.f * Scale;
	const float Length = (8.f + 6.f * Fade) * Scale;
	const FVector2D Diagonals[] = { FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, -1.f) };
	for (const FVector2D& Diagonal : Diagonals)
	{
		const FVector2D Direction = Diagonal.GetSafeNormal();
		const FVector2D Inner = Reticle + Direction * Gap;
		const FVector2D Outer = Reticle + Direction * (Gap + Length);
		DrawSegment(this, Inner, Outer, Color, 2.f * Scale);
	}
}

void AHeavyHUD::DrawDamageNumbers(AHeavyCharacter* Character)
{
	const float Now = Character->GetGameTime();
	UFont* Font = GEngine->GetMediumFont();
	const float Scale = UIScale();

	for (const FHeavyHitEvent& Event : Character->GetHitEvents())
	{
		const float Age = Now - Event.Time;
		if (Age < 0.f || Age > 1.f)
		{
			continue;
		}

		FVector2D Screen;
		if (!ProjectToScreen(Event.Location + FVector(0.f, 0.f, 30.f + Age * 70.f), Screen))
		{
			continue;
		}

		FLinearColor Color = Event.bKill ? HudColors::Kill : (Event.bCritical ? HudColors::Critical : HudColors::Text);
		Color.A = 1.f - Age * Age;
		const FString Label = FString::Printf(TEXT("%d%s"), FMath::RoundToInt(Event.Damage), Event.bKill ? TEXT("  DOWN") : TEXT(""));
		const float Pop = 1.f + 0.35f * FMath::Max(0.f, 1.f - Age * 6.f);
		DrawShadowedText(Label, Color, static_cast<float>(Screen.X), static_cast<float>(Screen.Y), Font, 1.1f * Scale * Pop, true);
	}
}

void AHeavyHUD::DrawTargetBars(AHeavyCharacter* Character)
{
	const FVector Viewer = Character->GetActorLocation();
	const float Scale = UIScale();
	UFont* Font = GEngine->GetSmallFont();

	for (TActorIterator<AHeavyTargetDummy> It(GetWorld()); It; ++It)
	{
		const AHeavyTargetDummy* Dummy = *It;
		const bool bDamaged = Dummy->GetHealth() < Dummy->GetMaxHealth();
		if (!bDamaged && !Dummy->IsDown())
		{
			continue;
		}
		if (FVector::DistSquared(Dummy->GetActorLocation(), Viewer) > 4500.f * 4500.f)
		{
			continue;
		}

		FVector2D Screen;
		if (!ProjectToScreen(Dummy->GetHealthBarLocation(), Screen))
		{
			continue;
		}

		const float ScreenX = static_cast<float>(Screen.X);
		const float ScreenY = static_cast<float>(Screen.Y);
		if (Dummy->IsDown())
		{
			DrawShadowedText(TEXT("DOWN"), HudColors::Dim, ScreenX, ScreenY, Font, Scale, true);
			continue;
		}

		const float Width = 56.f * Scale;
		const float Height = 5.f * Scale;
		const float Fraction = FMath::Clamp(Dummy->GetHealth() / FMath::Max(Dummy->GetMaxHealth(), 1.f), 0.f, 1.f);
		DrawRect(HudColors::Panel, ScreenX - Width * 0.5f - 1.f, ScreenY - 1.f, Width + 2.f, Height + 2.f);
		DrawRect(HudColors::HealthFill, ScreenX - Width * 0.5f, ScreenY, Width * Fraction, Height);
	}
}

void AHeavyHUD::DrawAmmo(AHeavyCharacter* Character)
{
	const AHeavyShotgun* Shotgun = Character->GetWeapon();
	if (!Shotgun)
	{
		return;
	}

	const float Scale = UIScale();
	const int32 Shells = Shotgun->GetShells();
	const int32 Capacity = Shotgun->GetCapacity();

	const float ShellWidth = 12.f * Scale;
	const float ShellHeight = 32.f * Scale;
	const float BrassHeight = 8.f * Scale;
	const float Gap = 6.f * Scale;
	const float Margin = 48.f * Scale;
	const float TotalWidth = Capacity * ShellWidth + (Capacity - 1) * Gap;
	const float Left = Canvas->ClipX - Margin - TotalWidth;
	const float Top = Canvas->ClipY - Margin - ShellHeight;

	DrawRect(HudColors::Panel, Left - 14.f * Scale, Top - 40.f * Scale, TotalWidth + 28.f * Scale, ShellHeight + 54.f * Scale);

	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		const bool bLoaded = Index < Shells;
		const float X = Left + Index * (ShellWidth + Gap);
		DrawRect(bLoaded ? HudColors::ShellHull : HudColors::ShellEmpty, X, Top, ShellWidth, ShellHeight - BrassHeight);
		DrawRect(bLoaded ? HudColors::ShellBrass : HudColors::ShellEmpty, X, Top + ShellHeight - BrassHeight, ShellWidth, BrassHeight);
	}

	FString Status = TEXT("PUMP SHOTGUN");
	FLinearColor StatusColor = HudColors::Dim;
	if (Shotgun->IsReloading())
	{
		Status = TEXT("RELOADING");
		StatusColor = HudColors::Critical;
	}
	else if (Shells == 0)
	{
		Status = TEXT("EMPTY - R");
		StatusColor = HudColors::Kill;
	}
	DrawShadowedText(Status, StatusColor, Left, Top - 30.f * Scale, GEngine->GetSmallFont(), 1.1f * Scale);
}

void AHeavyHUD::DrawHelp(AHeavyCharacter* Character)
{
	const float Scale = UIScale();
	UFont* Font = GEngine->GetSmallFont();
	const float X = 24.f * Scale;
	float Y = 20.f * Scale;
	const float Line = 18.f * Scale;

	if (!Character->ShouldShowHelp())
	{
		DrawShadowedText(TEXT("H: help"), HudColors::Dim, X, Y, Font, Scale);
		return;
	}

	const TCHAR* Lines[] =
	{
		TEXT("HEAVY SHOOTER PROTOTYPE"),
		TEXT("WASD move   Mouse look   LMB fire   RMB aim"),
		TEXT("Shift sprint   Space jump   C / Ctrl crouch   R reload"),
		TEXT("V swap shoulder   H hide this"),
		TEXT("Pad: sticks, RT fire, LT aim, L3 sprint, A jump, B crouch, X reload, R3 shoulder"),
	};
	for (const TCHAR* Text : Lines)
	{
		DrawShadowedText(Text, HudColors::Text, X, Y, Font, Scale);
		Y += Line;
	}

	// Live readout to help tune the feel.
	Y += Line * 0.5f;
	const float Speed = static_cast<float>(Character->GetVelocity().Size2D()) / 100.f;
	FString State = TEXT("jog");
	if (Character->IsSprinting())
	{
		State = TEXT("sprint");
	}
	else if (Character->GetAimAlpha() > 0.5f)
	{
		State = TEXT("aim");
	}
	if (Character->bIsCrouched)
	{
		State += TEXT(" + crouch");
	}
	if (!Character->GetCharacterMovement()->IsMovingOnGround())
	{
		State += TEXT(" + air");
	}

	DrawShadowedText(FString::Printf(TEXT("speed %.1f m/s   %s"), Speed, *State), HudColors::Dim, X, Y, Font, Scale);
	Y += Line;
	DrawShadowedText(FString::Printf(TEXT("weapon lag %.1f deg   spread %.1f deg"), Character->GetWeaponLagDegrees(), Character->GetCurrentSpread()),
		HudColors::Dim, X, Y, Font, Scale);
}
