// Body half of APFCharacter: the primitive skeleton, procedural walk cycle and leg IK.
// Weapon posing and arm IK live in PFCharacterArms.cpp.

#include "PFCharacter.h"
#include "PFShapes.h"
#include "PFShotgun.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const FLinearColor ArmorColor(0.82f, 0.82f, 0.8f);
	const FLinearColor ArmorShadeColor(0.6f, 0.61f, 0.62f);
	const FLinearColor SuitColor(0.07f, 0.07f, 0.08f);
	const FLinearColor VisorColor(0.01f, 0.01f, 0.012f);
	const FLinearColor AccentColor(0.12f, 0.28f, 0.6f);
	const FLinearColor BeltColor(0.18f, 0.16f, 0.12f);
	const FLinearColor ShellColor(0.75f, 0.08f, 0.05f);

	// Skeleton measurements in cm. Body space: X forward, Y right, Z up, origin on the ground between the feet.
	constexpr float StandPelvisHeight = 96.f;
	constexpr float CrouchPelvisHeight = 60.f;
	constexpr float SpineOffset = 8.f;
	constexpr float NeckOffset = 53.f;
	constexpr float ShoulderHeight = 48.f; // above the spine joint
	constexpr float ShoulderWidth = 25.f;
	constexpr float UpperArmLength = 30.f;
	constexpr float ForearmLength = 28.f;
	constexpr float HipWidth = 11.f;
	constexpr float HipDrop = 6.f;
	constexpr float ThighLength = 44.f;
	constexpr float ShinLength = 42.f;
	constexpr float AnkleHeight = 8.f;
	constexpr float FootSpread = 12.f;
}


void APFCharacter::BuildBody()
{
	if (BodyRoot)
	{
		return;
	}

	const FRotator Zero = FRotator::ZeroRotator;

	// Feet-level root that follows the capsule (repositioned every frame because crouching resizes the capsule)
	BodyRoot = FPFShapes::AddJoint(this, GetCapsuleComponent(), FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));

	// Hips
	Pelvis = FPFShapes::AddJoint(this, BodyRoot, FVector(0.f, 0.f, StandPelvisHeight));
	FPFShapes::AddPart(this, Pelvis, EPFShape::Cube, FVector(22.f, 34.f, 18.f), FVector::ZeroVector, Zero, SuitColor);
	FPFShapes::AddPart(this, Pelvis, EPFShape::Cube, FVector(25.f, 37.f, 6.f), FVector(0.f, 0.f, 7.f), Zero, BeltColor);
	FPFShapes::AddPart(this, Pelvis, EPFShape::Cube, FVector(8.f, 10.f, 9.f), FVector(13.f, -12.f, 4.f), Zero, BeltColor); // shell pouch

	// Torso
	Spine = FPFShapes::AddJoint(this, Pelvis, FVector(0.f, 0.f, SpineOffset));
	FPFShapes::AddPart(this, Spine, EPFShape::Cube, FVector(23.f, 40.f, 50.f), FVector(0.f, 0.f, 26.f), Zero, ArmorColor);
	FPFShapes::AddPart(this, Spine, EPFShape::Cube, FVector(5.f, 30.f, 24.f), FVector(12.f, 0.f, 34.f), Zero, ArmorShadeColor);
	FPFShapes::AddPart(this, Spine, EPFShape::Cube, FVector(5.f, 28.f, 12.f), FVector(12.f, 0.f, 13.f), Zero, SuitColor);
	FPFShapes::AddPart(this, Spine, EPFShape::Cube, FVector(13.f, 30.f, 34.f), FVector(-17.5f, 0.f, 29.f), Zero, SuitColor); // backpack
	FPFShapes::AddPart(this, Spine, EPFShape::Cube, FVector(3.f, 8.f, 8.f), FVector(-25.f, 9.f, 38.f), Zero, AccentColor);
	FPFShapes::AddPart(this, Spine, EPFShape::Sphere, FVector(17.f, 17.f, 15.f), FVector(0.f, -ShoulderWidth, ShoulderHeight + 2.f), Zero, ArmorColor);
	FPFShapes::AddPart(this, Spine, EPFShape::Sphere, FVector(17.f, 17.f, 15.f), FVector(0.f, ShoulderWidth, ShoulderHeight + 2.f), Zero, AccentColor);

	// Head (helmet, visor, mouth guard)
	Neck = FPFShapes::AddJoint(this, Spine, FVector(0.f, 0.f, NeckOffset));
	FPFShapes::AddPart(this, Neck, EPFShape::Cylinder, FVector(10.f, 10.f, 8.f), FVector(0.f, 0.f, 3.f), Zero, SuitColor);
	FPFShapes::AddPart(this, Neck, EPFShape::Sphere, FVector(25.f, 24.f, 28.f), FVector(0.f, 0.f, 16.f), Zero, ArmorColor);
	FPFShapes::AddPart(this, Neck, EPFShape::Cube, FVector(6.f, 18.f, 7.f), FVector(10.5f, 0.f, 17.f), Zero, VisorColor);
	FPFShapes::AddPart(this, Neck, EPFShape::Cube, FVector(5.f, 8.f, 8.f), FVector(10.f, 0.f, 8.f), Zero, ArmorShadeColor);

	BuildArm(ArmL, -1.f);
	BuildArm(ArmR, 1.f);
	BuildLeg(LegL, -1.f);
	BuildLeg(LegR, 1.f);

	// Shell carried in the left hand while reloading
	HeldShell = FPFShapes::AddPart(this, ArmL.Mid, EPFShape::Cylinder, FVector(2.4f, 2.4f, 7.f), FVector(0.f, 0.f, -ForearmLength - 4.f), Zero, ShellColor, false, false);
	HeldShell->SetVisibility(false);
}

void APFCharacter::BuildArm(FPFLimb& Arm, float Side)
{
	Arm.UpperLength = UpperArmLength;
	Arm.LowerLength = ForearmLength;

	Arm.Root = FPFShapes::AddJoint(this, Spine, FVector(0.f, Side * ShoulderWidth, ShoulderHeight));
	FPFShapes::AddPart(this, Arm.Root, EPFShape::Cylinder, FVector(11.f, 11.f, UpperArmLength), FVector(0.f, 0.f, -UpperArmLength * 0.5f), FRotator::ZeroRotator, SuitColor);

	Arm.Mid = FPFShapes::AddJoint(this, Arm.Root, FVector(0.f, 0.f, -UpperArmLength));
	FPFShapes::AddPart(this, Arm.Mid, EPFShape::Sphere, FVector(11.f, 11.f, 11.f), FVector::ZeroVector, FRotator::ZeroRotator, SuitColor);
	FPFShapes::AddPart(this, Arm.Mid, EPFShape::Cylinder, FVector(10.5f, 10.5f, ForearmLength * 0.75f), FVector(0.f, 0.f, -ForearmLength * 0.45f), FRotator::ZeroRotator, ArmorColor);
	FPFShapes::AddPart(this, Arm.Mid, EPFShape::Sphere, FVector(10.f, 10.f, 10.f), FVector(0.f, 0.f, -ForearmLength), FRotator::ZeroRotator, SuitColor); // hand
}

void APFCharacter::BuildLeg(FPFLimb& Leg, float Side)
{
	Leg.UpperLength = ThighLength;
	Leg.LowerLength = ShinLength;

	Leg.Root = FPFShapes::AddJoint(this, Pelvis, FVector(0.f, Side * HipWidth, -HipDrop));
	FPFShapes::AddPart(this, Leg.Root, EPFShape::Cylinder, FVector(15.f, 15.f, ThighLength), FVector(0.f, 0.f, -ThighLength * 0.5f), FRotator::ZeroRotator, SuitColor);
	FPFShapes::AddPart(this, Leg.Root, EPFShape::Cube, FVector(5.f, 12.f, 20.f), FVector(7.5f, 0.f, -ThighLength * 0.45f), FRotator::ZeroRotator, ArmorColor);

	Leg.Mid = FPFShapes::AddJoint(this, Leg.Root, FVector(0.f, 0.f, -ThighLength));
	FPFShapes::AddPart(this, Leg.Mid, EPFShape::Sphere, FVector(13.f, 13.f, 13.f), FVector::ZeroVector, FRotator::ZeroRotator, ArmorColor);
	FPFShapes::AddPart(this, Leg.Mid, EPFShape::Cylinder, FVector(13.f, 13.f, ShinLength), FVector(0.f, 0.f, -ShinLength * 0.5f), FRotator::ZeroRotator, ArmorColor);

	// Feet hang off the body root and are placed at the end of the leg every frame
	UStaticMeshComponent* Foot = FPFShapes::AddPart(this, BodyRoot, EPFShape::Cube, FVector(26.f, 12.f, 9.f), FVector(6.f, Side * FootSpread, 4.5f), FRotator::ZeroRotator, SuitColor);
	(Side < 0.f ? FootL : FootR) = Foot;
}

void APFCharacter::UpdateBody(float DeltaSeconds)
{
	if (!BodyRoot || !Pelvis || !Spine || !Neck)
	{
		return;
	}

	BodyRoot->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

	const FTransform BodyXf = BodyRoot->GetComponentTransform();
	const FQuat BodyQ = BodyXf.GetRotation();
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bInAir = Move && Move->IsFalling();

	// Velocity in body space drives the walk cycle (so strafing and backpedalling step the right way)
	const FVector LocalVel = BodyQ.UnrotateVector(GetVelocity());
	const float Speed = (float)FVector2D(LocalVel.X, LocalVel.Y).Size();
	const FVector MoveDir = Speed > 1.f ? FVector(LocalVel.X / Speed, LocalVel.Y / Speed, 0.f) : FVector::ForwardVector;
	const float MoveAlpha = bInAir ? 0.f : FMath::Clamp(Speed / 150.f, 0.f, 1.f);

	CrouchAlpha = FMath::FInterpTo(CrouchAlpha, bIsCrouched ? 1.f : 0.f, DeltaSeconds, 10.f);
	AirAlpha = FMath::FInterpTo(AirAlpha, bInAir ? 1.f : 0.f, DeltaSeconds, 8.f);

	// Stride grows with speed; the phase advances so planted feet don't slide
	const float StrideHalf = FMath::Clamp(25.f + Speed * 0.04f, 25.f, 48.f) * FMath::Lerp(1.f, 0.8f, CrouchAlpha);
	if (!bInAir && Speed > 5.f)
	{
		WalkPhase = FMath::Fmod(WalkPhase + DeltaSeconds * Speed * UE_PI / (2.f * StrideHalf), 2.f * UE_PI);
	}

	// Pelvis: bob and sway while moving, drop when crouched
	const float Bob = MoveAlpha * (FMath::Abs(FMath::Sin(WalkPhase)) * 3.5f + 3.f * SprintAlpha);
	const float PelvisZ = FMath::Lerp(StandPelvisHeight, CrouchPelvisHeight, CrouchAlpha) - Bob + AirAlpha * 3.f;
	const float PelvisYaw = 6.f * MoveAlpha * FMath::Sin(WalkPhase) * (1.f - 0.5f * CrouchAlpha);
	Pelvis->SetRelativeLocationAndRotation(FVector(0.f, 0.f, PelvisZ), FRotator(0.f, PelvisYaw, 0.f));

	// Spine: lean into sprints and crouches, follow aim pitch and blade the shoulders while the gun is up
	const float ControlPitch = (float)FRotator::NormalizeAxis(GetControlRotation().Pitch);
	const float ArmedAlpha = (1.f - HolsterAlpha) * (1.f - SprintAlpha);
	const float Lean = -12.f * SprintAlpha - 16.f * CrouchAlpha - 4.f * MoveAlpha * (1.f - SprintAlpha);
	const float SpinePitch = Lean + ControlPitch * 0.4f * ArmedAlpha;
	const float SpineYaw = 28.f * ArmedAlpha - PelvisYaw;
	Spine->SetRelativeRotation(FRotator(SpinePitch, SpineYaw, 0.f));

	// Head looks where the camera looks
	const float YawToCamera = (float)FRotator::NormalizeAxis(GetControlRotation().Yaw - GetActorRotation().Yaw);
	const float HeadYaw = FMath::Clamp(YawToCamera, -70.f, 70.f) - SpineYaw - PelvisYaw;
	const float HeadPitch = FMath::Clamp(ControlPitch * 0.9f - SpinePitch, -45.f, 45.f);
	Neck->SetRelativeRotation(FRotator(HeadPitch, HeadYaw, 0.f));

	// Legs: feet step along the movement direction, lift on the forward swing, tuck in the air
	for (int32 i = 0; i < 2; ++i)
	{
		const bool bLeft = i == 0;
		const float Side = bLeft ? -1.f : 1.f;
		const float Phase = WalkPhase + (bLeft ? 0.f : UE_PI);

		FVector FootLocal(0.f, Side * FootSpread, AnkleHeight);
		FootLocal.X += CrouchAlpha * (bLeft ? 16.f : -20.f);
		FootLocal += MoveDir * (FMath::Sin(Phase) * StrideHalf * MoveAlpha);
		FootLocal.Z += FMath::Max(0.f, FMath::Cos(Phase)) * 14.f * MoveAlpha;
		FootLocal = FMath::Lerp(FootLocal, FVector(bLeft ? 14.f : -14.f, Side * 13.f, 32.f), AirAlpha * 0.8f);

		const FVector Pole = BodyQ.RotateVector(FVector(1.f, Side * 0.15f, 0.f)); // knees bend forward
		const FVector Ankle = SolveLimb(bLeft ? LegL : LegR, BodyXf.TransformPosition(FootLocal), Pole);

		if (UStaticMeshComponent* Foot = bLeft ? FootL : FootR)
		{
			const FVector AnkleLocal = BodyXf.InverseTransformPosition(Ankle);
			Foot->SetRelativeLocationAndRotation(AnkleLocal + FVector(6.f, 0.f, 4.5f - AnkleHeight), FRotator::ZeroRotator);
		}
	}

	// Gun first (the arms reach for it), then the arms
	if (Weapon)
	{
		Weapon->SetActorTransform(ComputeWeaponPose());
	}
	UpdateArms(BodyXf, MoveAlpha);
}

FVector APFCharacter::SolveLimb(FPFLimb& Limb, const FVector& Target, const FVector& PoleDir)
{
	if (!Limb.Root || !Limb.Mid || !BodyRoot)
	{
		return Target;
	}

	// Rotations are built relative to the body so limb twist follows the character's facing
	const FQuat RefQ = BodyRoot->GetComponentQuat();
	auto PointDown = [&RefQ](const FVector& Dir)
	{
		const FVector Local = RefQ.UnrotateVector(Dir).GetSafeNormal();
		return Local.IsNearlyZero() ? RefQ : RefQ * FQuat::FindBetweenNormals(FVector(0.0, 0.0, -1.0), Local);
	};

	const FVector Start = Limb.Root->GetComponentLocation();
	const FVector ToTarget = Target - Start;
	const float RawDist = (float)ToTarget.Size();
	const FVector Dir = RawDist > UE_KINDA_SMALL_NUMBER ? ToTarget / RawDist : RefQ.RotateVector(FVector::DownVector);

	// Law of cosines for the angle at the root joint
	const float A = Limb.UpperLength;
	const float B = Limb.LowerLength;
	const float Dist = FMath::Clamp(RawDist, FMath::Abs(A - B) + 1.f, A + B - 0.5f);
	const float CosA = FMath::Clamp((A * A + Dist * Dist - B * B) / (2.f * A * Dist), -1.f, 1.f);
	const float SinA = FMath::Sqrt(FMath::Max(0.f, 1.f - CosA * CosA));

	FVector Bend = PoleDir - Dir * FVector::DotProduct(PoleDir, Dir);
	if (!Bend.Normalize())
	{
		Bend = FVector::CrossProduct(Dir, RefQ.GetRightVector());
		Bend.Normalize();
	}

	const FVector Mid = Start + Dir * (A * CosA) + Bend * (A * SinA);
	const FVector End = Start + Dir * Dist;
	Limb.Root->SetWorldRotation(PointDown(Mid - Start));
	Limb.Mid->SetWorldRotation(PointDown(End - Mid));
	return End;
}
