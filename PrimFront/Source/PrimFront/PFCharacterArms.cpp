// Arms half of APFCharacter: where the shotgun is held for every action, and two-bone IK
// that puts the hands on it (or swings them freely when it's holstered).

#include "PFCharacter.h"
#include "PFShapes.h"
#include "PFShotgun.h"
#include "Components/StaticMeshComponent.h"

namespace
{
	float Smooth(float X)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(X, 0.f, 1.f));
	}

	/** 0 -> 1 -> 0 over [0,1], ramping in over the first RampIn and out over the last RampOut. */
	float Envelope(float P, float RampIn, float RampOut)
	{
		if (P < RampIn)
		{
			return Smooth(P / RampIn);
		}
		if (P > 1.f - RampOut)
		{
			return Smooth((1.f - P) / RampOut);
		}
		return 1.f;
	}

	/** Transform for an offset given in a parent frame. */
	FTransform InFrame(const FTransform& Frame, const FVector& Location, const FRotator& Rotation)
	{
		return FTransform(Rotation, Location) * Frame;
	}

	/** Gun frame from its barrel direction (X) and its top (Z), expressed in a parent frame. */
	FTransform InFrameXZ(const FTransform& Frame, const FVector& Location, const FVector& XAxis, const FVector& ZAxis)
	{
		return FTransform(FRotationMatrix::MakeFromXZ(XAxis, ZAxis).ToQuat(), Location) * Frame;
	}
}

FTransform APFCharacter::ComputeWeaponPose() const
{
	const FQuat BodyQ = BodyRoot->GetComponentQuat();
	const FVector Pivot = ArmR.Root->GetComponentLocation(); // right shoulder
	const FTransform ShoulderFrame(BodyQ, Pivot);
	const FTransform SpineFrame = Spine->GetComponentTransform();

	// Hip fire / ADS: offsets from the right shoulder in a frame that looks at the aim point.
	// ADS lifts the stock into the shoulder and the sights towards the eye line.
	const FQuat AimQ = (AimPoint - Pivot).Rotation().Quaternion();
	const FVector HipGrip = Pivot + AimQ.RotateVector(FVector(22.f, 2.f, -27.f));
	const FVector AdsGrip = Pivot + AimQ.RotateVector(FVector(22.f, -9.f, -8.f));
	const FVector Grip = FMath::Lerp(HipGrip, AdsGrip, AimAlpha);
	FTransform Pose((AimPoint - Grip).Rotation().Quaternion(), Grip);

	// Reload: muzzle down a little, rolled so the loading port faces the left hand
	if (ReloadAlpha > 0.001f)
	{
		const float AimPitch = (float)FRotator::NormalizeAxis(GetControlRotation().Pitch);
		const FTransform Reload = InFrame(ShoulderFrame, FVector(24.f, -4.f, -26.f), FRotator(AimPitch * 0.3f - 12.f, -8.f, -40.f));
		Pose = FPFShapes::Blend(Pose, Reload, Smooth(ReloadAlpha));
	}

	// Sprint: carried diagonally across the chest
	if (SprintAlpha > 0.001f)
	{
		const FTransform Sprint = InFrame(ShoulderFrame, FVector(22.f, -14.f, -24.f), FRotator(28.f, -62.f, -10.f));
		Pose = FPFShapes::Blend(Pose, Sprint, Smooth(SprintAlpha));
	}

	// Armed melee: pull the gun across the chest, then swing the stock forward
	if (MeleeTimer > 0.f && bMeleeArmed)
	{
		const float P = GetMeleeProgress();
		const FTransform Windup = InFrame(ShoulderFrame, FVector(14.f, -8.f, -22.f), FRotator(20.f, -50.f, 0.f));
		const FTransform Strike = InFrame(ShoulderFrame, FVector(36.f, -6.f, -10.f), FRotator(10.f, -150.f, 0.f));
		if (P < 0.25f)
		{
			Pose = FPFShapes::Blend(Pose, Windup, Smooth(P / 0.25f));
		}
		else if (P < 0.42f)
		{
			Pose = FPFShapes::Blend(Windup, Strike, Smooth((P - 0.25f) / 0.17f));
		}
		else if (P < 0.6f)
		{
			Pose = Strike;
		}
		else
		{
			Pose = FPFShapes::Blend(Strike, Pose, Smooth((P - 0.6f) / 0.4f));
		}
	}

	// Recoil: kick back and up in the gun's own frame
	if (RecoilKick > 0.001f)
	{
		Pose = FTransform(FRotator(RecoilKick * 9.f, 0.f, 0.f), FVector(-RecoilKick * 7.f, 0.f, 0.f)) * Pose;
	}

	// Holster: up past the right shoulder, then down diagonally across the back
	if (HolsterAlpha > 0.001f)
	{
		const FTransform OverShoulder = InFrameXZ(SpineFrame, FVector(6.f, 32.f, 78.f), FVector(-0.25f, -0.15f, 1.f), FVector(-1.f, 0.f, -0.2f));
		const FTransform OnBack = InFrameXZ(SpineFrame, FVector(-30.f, 12.f, 12.f), FVector(0.f, -0.55f, 0.83f), FVector(-1.f, 0.f, 0.f));
		if (HolsterAlpha < 0.5f)
		{
			Pose = FPFShapes::Blend(Pose, OverShoulder, Smooth(HolsterAlpha / 0.5f));
		}
		else
		{
			Pose = FPFShapes::Blend(OverShoulder, OnBack, Smooth((HolsterAlpha - 0.5f) / 0.5f));
		}
	}

	return Pose;
}

void APFCharacter::UpdateArms(const FTransform& BodyXf, float MoveAlpha)
{
	if (!ArmL.Root || !ArmR.Root)
	{
		return;
	}

	const FQuat BodyQ = BodyXf.GetRotation();
	FPFLimb* Arms[2] = { &ArmL, &ArmR };
	FVector Targets[2];
	FVector Poles[2];

	// Free hands: swing opposite the legs, tuck up and pump harder when sprinting
	for (int32 i = 0; i < 2; ++i)
	{
		const float Side = i == 0 ? -1.f : 1.f;
		const FVector Shoulder = Arms[i]->Root->GetComponentLocation();
		const float Phase = WalkPhase + (i == 0 ? UE_PI : 0.f);
		const float Swing = FMath::Sin(Phase) * MoveAlpha * FMath::Lerp(16.f, 30.f, SprintAlpha);
		Targets[i] = Shoulder + BodyQ.RotateVector(FVector(Swing + 4.f, Side * 7.f, FMath::Lerp(-54.f, -40.f, SprintAlpha)));
		Poles[i] = BodyQ.RotateVector(FVector(-0.7f, Side * 0.5f, -0.3f));
	}

	bool bShowShell = false;
	if (Weapon)
	{
		FVector GripTarget = Weapon->GetGripLocation();
		FVector SupportTarget = Weapon->GetPumpLocation();

		// Reload: the left hand shuttles between the shell pouch and the loading port
		if (ReloadPhase != EPFReloadPhase::None)
		{
			const FVector Port = Weapon->GetLoadingPortLocation();
			const FVector Pouch = Pelvis->GetComponentTransform().TransformPosition(FVector(16.f, -12.f, 4.f));
			if (ReloadPhase == EPFReloadPhase::Start)
			{
				const float P = 1.f - FMath::Clamp(ReloadTimer / FMath::Max(ReloadStartTime, 0.01f), 0.f, 1.f);
				SupportTarget = FMath::Lerp(SupportTarget, Port, Smooth(P));
			}
			else if (ReloadPhase == EPFReloadPhase::Insert)
			{
				const float P = 1.f - FMath::Clamp(ReloadTimer / FMath::Max(ShellInsertTime, 0.01f), 0.f, 1.f);
				if (P < 0.35f)
				{
					SupportTarget = FMath::Lerp(Port, Pouch, Smooth(P / 0.35f));
				}
				else if (P < 0.75f)
				{
					SupportTarget = FMath::Lerp(Pouch, Port, Smooth((P - 0.35f) / 0.4f));
				}
				else
				{
					// Thumb the shell up into the tube
					SupportTarget = Port + Weapon->GetActorUpVector() * (3.f * Smooth((P - 0.75f) / 0.25f));
				}
				bShowShell = P > 0.3f && P < 0.85f;
			}
		}

		// Holstering: the left hand lets go first, the right hand carries the gun to the back
		const float RightOnGun = 1.f - Smooth((HolsterAlpha - 0.7f) / 0.3f);
		const float LeftOnGun = 1.f - Smooth(HolsterAlpha / 0.3f);

		const FVector GripPole = BodyQ.RotateVector(FVector(-0.2f, 0.8f, -1.f));
		const FVector SupportPole = BodyQ.RotateVector(FVector(-0.1f, -0.8f, -1.f));

		Targets[1] = FMath::Lerp(Targets[1], GripTarget, RightOnGun);
		Poles[1] = FMath::Lerp(Poles[1], GripPole, RightOnGun);
		Targets[0] = FMath::Lerp(Targets[0], SupportTarget, LeftOnGun);
		Poles[0] = FMath::Lerp(Poles[0], SupportPole, LeftOnGun);
	}

	// Unarmed melee: fists up into a guard, one hand jabs forward
	if (MeleeTimer > 0.f && !bMeleeArmed)
	{
		const float P = GetMeleeProgress();
		const float Guard = Envelope(P, 0.15f, 0.25f);
		const float Extend = P < 0.3f ? Smooth(P / 0.3f) : (P < 0.5f ? 1.f : 1.f - Smooth((P - 0.5f) / 0.5f));

		for (int32 i = 0; i < 2; ++i)
		{
			const float Side = i == 0 ? -1.f : 1.f;
			const bool bPunching = (i == 0) == bPunchLeft;
			const FVector Shoulder = Arms[i]->Root->GetComponentLocation();

			FVector Fist = Shoulder + BodyQ.RotateVector(FVector(24.f, -Side * 10.f, -8.f));
			float Weight = Guard;
			if (bPunching)
			{
				Fist = FMath::Lerp(Fist, Shoulder + BodyQ.RotateVector(FVector(56.f, -Side * 14.f, -4.f)), Extend);
				Weight = FMath::Max(Guard, Extend);
			}
			Targets[i] = FMath::Lerp(Targets[i], Fist, Weight);
			Poles[i] = FMath::Lerp(Poles[i], BodyQ.RotateVector(FVector(-0.3f, Side, -0.6f)), Weight);
		}
	}

	SolveLimb(ArmL, Targets[0], Poles[0]);
	SolveLimb(ArmR, Targets[1], Poles[1]);

	if (HeldShell)
	{
		HeldShell->SetVisibility(bShowShell);
	}
}
