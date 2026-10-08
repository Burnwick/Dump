// Combat half of APFCharacter: firing, shell-by-shell reload, melee and holstering.

#include "PFCharacter.h"
#include "PFShotgun.h"
#include "PFTarget.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float FireBufferTime = 0.3f;       // a click this early still fires when the pump is done
	constexpr float ReloadInterruptBuffer = 1.f; // a click during reload fires once it stops
	constexpr float HitMarkerDuration = 0.25f;
}

void APFCharacter::UpdateCombat(float DeltaSeconds)
{
	FireCooldown = FMath::Max(0.f, FireCooldown - DeltaSeconds);
	FireQueueTime = FMath::Max(0.f, FireQueueTime - DeltaSeconds);
	HitMarkerTime = FMath::Max(0.f, HitMarkerTime - DeltaSeconds);
	TimeSinceShot += DeltaSeconds;
	RecoilKick = FMath::FInterpTo(RecoilKick, 0.f, DeltaSeconds, 12.f);
	SpreadBloom = FMath::FInterpTo(SpreadBloom, 0.f, DeltaSeconds, 3.f);

	HolsterAlpha = FMath::FInterpConstantTo(HolsterAlpha, bHolstered ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(HolsterTime, 0.05f));

	if (MeleeTimer > 0.f)
	{
		MeleeTimer = FMath::Max(0.f, MeleeTimer - DeltaSeconds);
		const float ImpactAt = bMeleeArmed ? 0.42f : 0.3f;
		if (!bMeleeHitDone && GetMeleeProgress() >= ImpactAt)
		{
			bMeleeHitDone = true;
			DoMeleeHit();
		}
	}

	if (ReloadPhase != EPFReloadPhase::None)
	{
		TickReload(DeltaSeconds);
	}
	ReloadAlpha = FMath::FInterpTo(ReloadAlpha, ReloadPhase != EPFReloadPhase::None ? 1.f : 0.f, DeltaSeconds, 10.f);

	if (FireQueueTime > 0.f)
	{
		TryFire();
	}

	// Reload automatically once the tube is empty
	if (Weapon && Weapon->GetAmmo() == 0 && FireCooldown <= 0.f)
	{
		StartReload();
	}
}

void APFCharacter::OnFirePressed()
{
	// Pulling the trigger with the gun away draws it, like Battlefront
	if (bHolstered)
	{
		SetHolstered(false);
		return;
	}

	// Firing during a reload stops it after the current shell
	if (ReloadPhase != EPFReloadPhase::None)
	{
		if (Weapon && Weapon->GetAmmo() > 0)
		{
			bReloadInterrupt = true;
			FireQueueTime = ReloadInterruptBuffer;
		}
		return;
	}

	// Shooting breaks a sprint; press sprint again to resume
	bSprintHeld = false;
	FireQueueTime = FireBufferTime;
	TryFire();
}

bool APFCharacter::CanFireNow() const
{
	return Weapon && !bHolstered && HolsterAlpha <= 0.05f && MeleeTimer <= 0.f && FireCooldown <= 0.f
		&& ReloadPhase == EPFReloadPhase::None && SprintAlpha < 0.35f;
}

bool APFCharacter::TryFire()
{
	if (!CanFireNow())
	{
		return false;
	}

	FireQueueTime = 0.f;
	if (Weapon->GetAmmo() <= 0)
	{
		StartReload();
		return false;
	}

	FireShot();
	return true;
}

void APFCharacter::FireShot()
{
	FVector ViewLoc = FollowCamera->GetComponentLocation();
	FRotator ViewRot = FollowCamera->GetComponentRotation();
	if (Controller)
	{
		Controller->GetPlayerViewPoint(ViewLoc, ViewRot);
	}

	// Pellets travel down the crosshair line, starting level with the character
	const FVector Dir = ViewRot.Vector();
	const double Along = FMath::Max(FVector::DotProduct(GetActorLocation() - ViewLoc, Dir), 0.0);
	const FVector Start = ViewLoc + Dir * Along;

	const FPFShotResult Result = Weapon->FireShot(Start, Dir, GetSpreadDegrees(), Controller);

	++ShotsFired;
	PelletsFired += Result.Pellets;
	PelletsHit += Result.PelletsOnTarget;
	Kills += Result.Kills;
	Headshots += Result.bHeadshot ? 1 : 0;
	if (Result.PelletsOnTarget > 0)
	{
		RegisterHit(Result.Kills > 0, Result.bHeadshot);
	}

	FireCooldown = Weapon->GetFireInterval();
	TimeSinceShot = 0.f;
	RecoilKick = 1.f;
	SpreadBloom = FMath::Min(SpreadBloom + 1.f, 2.5f);

	// Camera kick
	if (Controller)
	{
		FRotator Control = Controller->GetControlRotation();
		Control.Pitch = FRotator::ClampAxis(Control.Pitch + FMath::Lerp(RecoilPitchHip, RecoilPitchAds, AimAlpha));
		Control.Yaw += FMath::FRandRange(-RecoilYaw, RecoilYaw);
		Controller->SetControlRotation(Control);
	}
}

float APFCharacter::GetSpreadDegrees() const
{
	float Spread = FMath::Lerp(HipSpread, AdsSpread, AimAlpha);
	if (bIsCrouched)
	{
		Spread *= 0.85f;
	}

	const float SpeedFrac = FMath::Clamp((float)GetVelocity().Size2D() / FMath::Max(RunSpeed, 1.f), 0.f, 1.f);
	Spread *= 1.f + 0.25f * SpeedFrac;

	if (GetCharacterMovement() && GetCharacterMovement()->IsFalling())
	{
		Spread *= 1.4f;
	}
	return Spread + SpreadBloom;
}

void APFCharacter::RegisterHit(bool bKill, bool bHead)
{
	HitMarkerTime = HitMarkerDuration;
	bHitMarkerKill = bKill;
	bHitMarkerHead = bHead;
}

// ---------------------------------------------------------------------------
// Reload: start -> insert shells one at a time -> (rack the pump if it was empty)
// ---------------------------------------------------------------------------

void APFCharacter::StartReload()
{
	if (!Weapon || !Weapon->CanReload() || ReloadPhase != EPFReloadPhase::None)
	{
		return;
	}
	if (bHolstered || HolsterAlpha > 0.05f || MeleeTimer > 0.f || FireCooldown > 0.f || bSprinting)
	{
		return;
	}

	ReloadPhase = EPFReloadPhase::Start;
	ReloadTimer = ReloadStartTime;
	bReloadInterrupt = false;
	bReloadNeedsPump = Weapon->GetAmmo() == 0;
}

void APFCharacter::CancelReload()
{
	ReloadPhase = EPFReloadPhase::None;
	ReloadTimer = 0.f;
	bReloadInterrupt = false;
}

void APFCharacter::TickReload(float DeltaSeconds)
{
	ReloadTimer -= DeltaSeconds;
	if (ReloadTimer > 0.f)
	{
		return;
	}

	switch (ReloadPhase)
	{
	case EPFReloadPhase::Start:
		ReloadPhase = EPFReloadPhase::Insert;
		ReloadTimer += ShellInsertTime;
		break;

	case EPFReloadPhase::Insert:
		if (Weapon)
		{
			Weapon->InsertShell();
		}
		if (bReloadInterrupt || !Weapon || !Weapon->CanReload())
		{
			if (bReloadNeedsPump)
			{
				ReloadPhase = EPFReloadPhase::Finish;
				ReloadTimer += ReloadFinishTime;
			}
			else
			{
				CancelReload();
			}
		}
		else
		{
			ReloadTimer += ShellInsertTime;
		}
		break;

	default:
		CancelReload();
		break;
	}
}

float APFCharacter::GetPumpAlpha() const
{
	// Rack the pump at the end of a reload from empty...
	if (ReloadPhase == EPFReloadPhase::Finish)
	{
		const float P = 1.f - FMath::Clamp(ReloadTimer / FMath::Max(ReloadFinishTime, 0.01f), 0.f, 1.f);
		return FMath::Sin(P * UE_PI);
	}

	// ...and shortly after every shot
	const float Interval = Weapon ? Weapon->GetFireInterval() : 0.85f;
	const float PumpStart = Interval * 0.18f;
	const float PumpLength = Interval * 0.55f;
	if (TimeSinceShot >= PumpStart && TimeSinceShot <= PumpStart + PumpLength)
	{
		return FMath::Sin((TimeSinceShot - PumpStart) / PumpLength * UE_PI);
	}
	return 0.f;
}

// ---------------------------------------------------------------------------
// Melee: rifle butt with the gun out, alternating punches when holstered
// ---------------------------------------------------------------------------

float APFCharacter::GetMeleeProgress() const
{
	return MeleeDuration > 0.f ? 1.f - FMath::Clamp(MeleeTimer / MeleeDuration, 0.f, 1.f) : 1.f;
}

void APFCharacter::StartMelee()
{
	if (MeleeTimer > 0.f)
	{
		return;
	}

	CancelReload();
	FireQueueTime = 0.f;
	bMeleeArmed = !bHolstered && HolsterAlpha < 0.5f;
	MeleeDuration = bMeleeArmed ? ArmedMeleeTime : UnarmedMeleeTime;
	MeleeTimer = MeleeDuration;
	bMeleeHitDone = false;
	if (!bMeleeArmed)
	{
		bPunchLeft = !bPunchLeft;
	}

	// Strike where the camera is looking
	if (Controller)
	{
		SetActorRotation(FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f));
	}
}

void APFCharacter::DoMeleeHit()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Forward = GetActorForwardVector();
	const FVector Center = GetActorLocation() + Forward * (MeleeRange * 0.6f) + FVector(0.f, 0.f, 20.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PFMelee), false, this);
	if (Weapon)
	{
		Params.AddIgnoredActor(Weapon);
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(MeleeRadius), Params);

	const float Damage = bMeleeArmed ? ArmedMeleeDamage : UnarmedMeleeDamage;
	TSet<AActor*> Damaged;
	bool bHitSomething = false;
	bool bKill = false;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Comp = Overlap.GetComponent();
		if (Comp && Comp->IsSimulatingPhysics())
		{
			Comp->AddImpulse(Forward * MeleePush + FVector(0.f, 0.f, MeleePush * 0.35f), NAME_None, true);
			bHitSomething = true;
		}

		APFTarget* Target = Cast<APFTarget>(Overlap.GetActor());
		if (!Target || Damaged.Contains(Target))
		{
			continue;
		}
		Damaged.Add(Target);

		if (!Target->IsDown())
		{
			UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, UDamageType::StaticClass());
			bHitSomething = true;
			++MeleeHits;
			if (Target->IsDown())
			{
				bKill = true;
				++Kills;
			}
		}
	}

	if (bHitSomething)
	{
		RegisterHit(bKill, false);
	}
}

// ---------------------------------------------------------------------------
// Holster
// ---------------------------------------------------------------------------

void APFCharacter::SetHolstered(bool bNewHolstered)
{
	if (MeleeTimer > 0.f || bHolstered == bNewHolstered)
	{
		return;
	}

	CancelReload();
	FireQueueTime = 0.f;
	bHolstered = bNewHolstered;
}
