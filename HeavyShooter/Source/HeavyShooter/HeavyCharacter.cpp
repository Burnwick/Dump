#include "HeavyCharacter.h"

#include "HeavyPartComponent.h"
#include "HeavyShooter.h"
#include "HeavyShotgun.h"
#include "HeavyTargetDummy.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace TrooperColors
{
	const FLinearColor Suit(0.075f, 0.08f, 0.085f);
	const FLinearColor Armor(0.26f, 0.29f, 0.22f);
	const FLinearColor Gear(0.16f, 0.17f, 0.13f);
	const FLinearColor Boot(0.06f, 0.05f, 0.04f);
	const FLinearColor Visor(0.05f, 0.22f, 0.28f);
	const FLinearColor Glove(0.05f, 0.05f, 0.05f);
	const FLinearColor ShellRed(0.55f, 0.05f, 0.04f);
}

namespace
{
	constexpr float AimTraceRange = 20000.f;
	constexpr float UpperArmLength = 32.f;
	constexpr float ForearmLength = 30.f;
	constexpr float StandingHipHeight = 92.f;
	constexpr float CrouchHipDrop = 29.5f;

	/** Stretches a Z-up cylinder part between two world points. */
	void PlaceSegment(USceneComponent* Segment, const FVector& From, const FVector& To, float Thickness)
	{
		const FVector Delta = To - From;
		const float Length = FMath::Max(static_cast<float>(Delta.Size()), 1.f);
		const FVector Direction = Delta / Length;
		Segment->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromZ(Direction).Rotator());
		// A little extra length so the joints overlap instead of showing gaps.
		Segment->SetWorldScale3D(FVector(Thickness, Thickness, Length + Thickness * 0.4f) / 100.f);
	}

	/** Two-bone IK: where the elbow goes so the hand reaches Hand, bending towards Pole. */
	FVector SolveElbow(const FVector& Shoulder, const FVector& Hand, float UpperLength, float LowerLength, const FVector& Pole)
	{
		const FVector ToHand = Hand - Shoulder;
		const float RawDistance = static_cast<float>(ToHand.Size());
		if (RawDistance < KINDA_SMALL_NUMBER)
		{
			return Shoulder + Pole.GetSafeNormal() * UpperLength;
		}

		const FVector Direction = ToHand / RawDistance;
		const float Distance = FMath::Clamp(RawDistance, FMath::Abs(UpperLength - LowerLength) + 0.5f, UpperLength + LowerLength - 0.5f);
		const float CosAngle = (UpperLength * UpperLength + Distance * Distance - LowerLength * LowerLength) / (2.f * UpperLength * Distance);
		const float Angle = FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f));

		FVector Bend = Pole - Direction * FVector::DotProduct(Pole, Direction);
		if (!Bend.Normalize())
		{
			Bend = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
		}
		return Shoulder + Direction * (FMath::Cos(Angle) * UpperLength) + Bend * (FMath::Sin(Angle) * UpperLength);
	}

	/** Frame-rate independent exponential smoothing factor. */
	float SmoothingAlpha(float Rate, float DeltaTime)
	{
		return 1.f - FMath::Exp(-Rate * DeltaTime);
	}
}

// =========================================================================================
// Construction
// =========================================================================================

AHeavyCharacter::AHeavyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	// Pose the body after movement has run this frame.
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	WeaponClass = AHeavyShotgun::StaticClass();

	GetCapsuleComponent()->InitCapsuleSize(36.f, 92.f);

	// Rotation is driven by the body-turn spring in Tick, never snapped to the controller.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->bUseControllerDesiredRotation = false;
	Move->MaxWalkSpeed = JogSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->MaxAcceleration = JogAcceleration;
	// Low friction makes direction changes carry momentum; braking still stops you in ~0.25s.
	Move->GroundFriction = 4.f;
	Move->BrakingFrictionFactor = 1.f;
	Move->BrakingDecelerationWalking = 1100.f;
	Move->JumpZVelocity = 520.f;
	Move->GravityScale = 1.6f;
	Move->AirControl = 0.15f;
	Move->BrakingDecelerationFalling = 150.f;
	Move->Mass = 140.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(62.f);
	Move->bCanWalkOffLedgesWhenCrouching = true;

	// The skeletal mesh slot stays empty: the trooper is assembled from primitives under it,
	// which keeps ACharacter's crouch mesh offsets working.
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	SpringArm->TargetArmLength = HipArmLength;
	SpringArm->SocketOffset = HipCameraOffset;
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = CameraLagSpeedHip;
	SpringArm->CameraLagMaxDistance = 80.f;
	SpringArm->bEnableCameraRotationLag = false;
	SpringArm->ProbeSize = 14.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(HipFOV);

	BuildBody();
}

USceneComponent* AHeavyCharacter::MakeJoint(FName Name, USceneComponent* Parent, const FVector& Location)
{
	USceneComponent* Joint = CreateDefaultSubobject<USceneComponent>(Name);
	Joint->SetupAttachment(Parent);
	Joint->SetRelativeLocation(Location);
	return Joint;
}

void AHeavyCharacter::BuildBody()
{
	using namespace TrooperColors;
	using P = UHeavyPartComponent;

	// Body space: X forward, Y right, Z up, origin between the feet. ~1.8 m tall.
	BodyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BodyRoot"));
	BodyRoot->SetupAttachment(GetMesh());

	Pelvis = MakeJoint(TEXT("Pelvis"), BodyRoot, FVector(0.f, 0.f, StandingHipHeight));
	P::CreatePart(this, TEXT("Hips"), Pelvis, EHeavyShape::Cube, FVector(0.f, 0.f, -2.f), FVector(22.f, 34.f, 18.f), FRotator::ZeroRotator, Suit);
	P::CreatePart(this, TEXT("Belt"), Pelvis, EHeavyShape::Cube, FVector(0.f, 0.f, 6.f), FVector(24.f, 36.f, 4.f), FRotator::ZeroRotator, Gear);
	P::CreatePart(this, TEXT("BeltPouchL"), Pelvis, EHeavyShape::Cube, FVector(7.f, -15.f, 1.f), FVector(10.f, 7.f, 11.f), FRotator::ZeroRotator, Gear);
	P::CreatePart(this, TEXT("BeltPouchR"), Pelvis, EHeavyShape::Cube, FVector(7.f, 15.f, 1.f), FVector(10.f, 7.f, 11.f), FRotator::ZeroRotator, Gear);
	ReloadPouch = MakeJoint(TEXT("ReloadPouch"), Pelvis, FVector(9.f, -17.f, -2.f));

	// Legs: hip -> knee -> ankle, posed procedurally in UpdateTorsoAndLegs.
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bLeft = (Side == 0);
		const TCHAR* Tag = bLeft ? TEXT("L") : TEXT("R");
		const float Y = bLeft ? -10.5f : 10.5f;

		USceneComponent* Thigh = MakeJoint(*FString::Printf(TEXT("Thigh%s"), Tag), Pelvis, FVector(0.f, Y, -2.f));
		P::CreatePart(this, *FString::Printf(TEXT("ThighMesh%s"), Tag), Thigh, EHeavyShape::Cylinder, FVector(0.f, 0.f, -22.f), FVector(15.f, 15.f, 46.f), FRotator::ZeroRotator, Suit);
		P::CreatePart(this, *FString::Printf(TEXT("ThighPlate%s"), Tag), Thigh, EHeavyShape::Cube, FVector(6.f, 0.f, -16.f), FVector(5.f, 13.f, 20.f), FRotator::ZeroRotator, Armor);

		USceneComponent* Knee = MakeJoint(*FString::Printf(TEXT("Knee%s"), Tag), Thigh, FVector(0.f, 0.f, -44.f));
		P::CreatePart(this, *FString::Printf(TEXT("Shin%s"), Tag), Knee, EHeavyShape::Cylinder, FVector(0.f, 0.f, -19.f), FVector(13.f, 13.f, 40.f), FRotator::ZeroRotator, Suit);
		P::CreatePart(this, *FString::Printf(TEXT("KneePad%s"), Tag), Knee, EHeavyShape::Cube, FVector(7.f, 0.f, 0.f), FVector(6.f, 12.f, 11.f), FRotator::ZeroRotator, Armor);
		P::CreatePart(this, *FString::Printf(TEXT("ShinGuard%s"), Tag), Knee, EHeavyShape::Cube, FVector(6.f, 0.f, -18.f), FVector(4.f, 11.f, 24.f), FRotator::ZeroRotator, Armor);

		USceneComponent* Ankle = MakeJoint(*FString::Printf(TEXT("Ankle%s"), Tag), Knee, FVector(0.f, 0.f, -38.f));
		P::CreatePart(this, *FString::Printf(TEXT("Boot%s"), Tag), Ankle, EHeavyShape::Cube, FVector(5.f, 0.f, -3.f), FVector(27.f, 13.f, 10.f), FRotator::ZeroRotator, Boot);

		if (bLeft)
		{
			ThighL = Thigh;
			KneeL = Knee;
			AnkleL = Ankle;
		}
		else
		{
			ThighR = Thigh;
			KneeR = Knee;
			AnkleR = Ankle;
		}
	}

	// Torso: leans, twists and blades in UpdateTorsoAndLegs.
	Spine = MakeJoint(TEXT("Spine"), Pelvis, FVector(0.f, 0.f, 8.f));
	P::CreatePart(this, TEXT("Torso"), Spine, EHeavyShape::Cube, FVector(0.f, 0.f, 24.f), FVector(24.f, 36.f, 50.f), FRotator::ZeroRotator, Suit);
	P::CreatePart(this, TEXT("ChestPlate"), Spine, EHeavyShape::Cube, FVector(13.f, 0.f, 31.f), FVector(8.f, 34.f, 28.f), FRotator::ZeroRotator, Armor);
	P::CreatePart(this, TEXT("Abdomen"), Spine, EHeavyShape::Cube, FVector(12.f, 0.f, 9.f), FVector(6.f, 28.f, 12.f), FRotator::ZeroRotator, Armor);
	P::CreatePart(this, TEXT("ChestPouches"), Spine, EHeavyShape::Cube, FVector(17.5f, 0.f, 25.f), FVector(4.f, 26.f, 9.f), FRotator::ZeroRotator, Gear);
	P::CreatePart(this, TEXT("Backpack"), Spine, EHeavyShape::Cube, FVector(-20.f, 0.f, 28.f), FVector(18.f, 30.f, 36.f), FRotator::ZeroRotator, Gear);
	P::CreatePart(this, TEXT("Bedroll"), Spine, EHeavyShape::Cylinder, FVector(-20.f, 0.f, 49.f), FVector(10.f, 10.f, 30.f), FRotator(0.f, 0.f, 90.f), Armor);
	P::CreatePart(this, TEXT("Antenna"), Spine, EHeavyShape::Cylinder, FVector(-25.f, 11.f, 66.f), FVector(1.2f, 1.2f, 44.f), FRotator::ZeroRotator, Suit);
	P::CreatePart(this, TEXT("ShoulderPadL"), Spine, EHeavyShape::Cube, FVector(0.f, -21.f, 48.f), FVector(20.f, 14.f, 9.f), FRotator(0.f, 0.f, -18.f), Armor);
	P::CreatePart(this, TEXT("ShoulderPadR"), Spine, EHeavyShape::Cube, FVector(0.f, 21.f, 48.f), FVector(20.f, 14.f, 9.f), FRotator(0.f, 0.f, 18.f), Armor);
	ShoulderL = MakeJoint(TEXT("ShoulderL"), Spine, FVector(0.f, -18.f, 45.f));
	ShoulderR = MakeJoint(TEXT("ShoulderR"), Spine, FVector(0.f, 18.f, 45.f));

	Neck = MakeJoint(TEXT("Neck"), Spine, FVector(0.f, 0.f, 51.f));
	P::CreatePart(this, TEXT("NeckMesh"), Neck, EHeavyShape::Cylinder, FVector(0.f, 0.f, 2.f), FVector(11.f, 11.f, 8.f), FRotator::ZeroRotator, Suit);
	P::CreatePart(this, TEXT("Head"), Neck, EHeavyShape::Sphere, FVector(1.f, 0.f, 12.f), FVector(20.f, 19.f, 22.f), FRotator::ZeroRotator, Suit);
	P::CreatePart(this, TEXT("Helmet"), Neck, EHeavyShape::Sphere, FVector(-1.f, 0.f, 15.5f), FVector(27.f, 25.f, 21.f), FRotator::ZeroRotator, Armor);
	P::CreatePart(this, TEXT("HelmetBrim"), Neck, EHeavyShape::Cube, FVector(9.f, 0.f, 19.f), FVector(8.f, 24.f, 3.f), FRotator(-8.f, 0.f, 0.f), Armor);
	P::CreatePart(this, TEXT("Visor"), Neck, EHeavyShape::Cube, FVector(10.5f, 0.f, 11.f), FVector(5.f, 19.f, 7.f), FRotator::ZeroRotator, Visor);
	P::CreatePart(this, TEXT("Rebreather"), Neck, EHeavyShape::Cube, FVector(10.f, 0.f, 4.f), FVector(6.f, 9.f, 6.f), FRotator::ZeroRotator, Gear);

	// The gun is held around a pivot in the chest.
	AimPivot = MakeJoint(TEXT("AimPivot"), Spine, FVector(0.f, 0.f, 44.f));
	WeaponMount = MakeJoint(TEXT("WeaponMount"), AimPivot, HipWeaponOffset);

	// Arms are re-placed every frame by IK; these are just starting transforms.
	UpperArmL = P::CreatePart(this, TEXT("UpperArmL"), BodyRoot, EHeavyShape::Cylinder, FVector(0.f, -22.f, 130.f), FVector(11.f, 11.f, UpperArmLength), FRotator::ZeroRotator, Suit);
	UpperArmR = P::CreatePart(this, TEXT("UpperArmR"), BodyRoot, EHeavyShape::Cylinder, FVector(0.f, 22.f, 130.f), FVector(11.f, 11.f, UpperArmLength), FRotator::ZeroRotator, Suit);
	ForearmL = P::CreatePart(this, TEXT("ForearmL"), BodyRoot, EHeavyShape::Cylinder, FVector(10.f, -22.f, 110.f), FVector(10.f, 10.f, ForearmLength), FRotator::ZeroRotator, Suit);
	ForearmR = P::CreatePart(this, TEXT("ForearmR"), BodyRoot, EHeavyShape::Cylinder, FVector(10.f, 22.f, 110.f), FVector(10.f, 10.f, ForearmLength), FRotator::ZeroRotator, Suit);
	ElbowL = P::CreatePart(this, TEXT("ElbowL"), BodyRoot, EHeavyShape::Sphere, FVector(0.f, -22.f, 115.f), FVector(12.f), FRotator::ZeroRotator, Armor);
	ElbowR = P::CreatePart(this, TEXT("ElbowR"), BodyRoot, EHeavyShape::Sphere, FVector(0.f, 22.f, 115.f), FVector(12.f), FRotator::ZeroRotator, Armor);
	HandL = P::CreatePart(this, TEXT("HandL"), BodyRoot, EHeavyShape::Cube, FVector(20.f, -22.f, 110.f), FVector(9.f, 7.f, 9.f), FRotator::ZeroRotator, Glove);
	HandR = P::CreatePart(this, TEXT("HandR"), BodyRoot, EHeavyShape::Cube, FVector(20.f, 22.f, 110.f), FVector(9.f, 7.f, 9.f), FRotator::ZeroRotator, Glove);

	HandShell = P::CreatePart(this, TEXT("HandShell"), BodyRoot, EHeavyShape::Cylinder, FVector(20.f, -22.f, 110.f), FVector(2.f, 2.f, 6.4f), FRotator::ZeroRotator, ShellRed);
	HandShell->SetCastShadow(false);
	HandShell->SetVisibility(false);
}

void AHeavyCharacter::BeginPlay()
{
	Super::BeginPlay();

	const float Yaw = static_cast<float>(GetActorRotation().Yaw);
	BodyYaw.Reset(Yaw);
	AimYaw.Reset(Yaw);
	AimPitch.Reset(0.f);
	ShoulderSide.Reset(ShoulderSign);
	LastVelocity = GetVelocity();

	// Let the camera boom read this frame's arm length / offsets after we set them.
	SpringArm->AddTickPrerequisiteActor(this);

	if (WeaponClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Weapon = GetWorld()->SpawnActor<AHeavyShotgun>(WeaponClass, WeaponMount->GetComponentTransform(), Params);
		if (Weapon)
		{
			Weapon->AttachToComponent(WeaponMount, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}

	const FVector Ahead = GetActorLocation() + GetActorForwardVector() * 1000.f;
	AimPoint = Ahead;
	VisualAimPoint = Ahead;
}

// =========================================================================================
// Input
// =========================================================================================

void AHeavyCharacter::BuildInput()
{
	if (InputContext)
	{
		return;
	}

	InputContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Heavy"));

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
	FireAction = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	AimAction = MakeAction(TEXT("IA_Aim"), EInputActionValueType::Boolean);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	SprintToggleAction = MakeAction(TEXT("IA_SprintToggle"), EInputActionValueType::Boolean);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	ReloadAction = MakeAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
	ShoulderAction = MakeAction(TEXT("IA_SwapShoulder"), EInputActionValueType::Boolean);
	HelpAction = MakeAction(TEXT("IA_ToggleHelp"), EInputActionValueType::Boolean);

	// Note: MapKey returns a reference into an array that the next MapKey may reallocate,
	// so each mapping is finished before the next one is added.
	auto Map = [this](UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate, bool bDeadZone)
	{
		FEnhancedActionKeyMapping& Mapping = InputContext->MapKey(Action, Key);
		if (bSwizzle)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(InputContext);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(InputContext));
		}
		if (bDeadZone)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(InputContext));
		}
	};
	auto MapButton = [&Map](UInputAction* Action, const FKey& Key)
	{
		Map(Action, Key, false, false, false);
	};

	// Move: X = right, Y = forward.
	Map(MoveAction, EKeys::W, true, false, false);
	Map(MoveAction, EKeys::S, true, true, false);
	Map(MoveAction, EKeys::D, false, false, false);
	Map(MoveAction, EKeys::A, false, true, false);
	Map(MoveAction, EKeys::Gamepad_Left2D, false, false, true);

	Map(LookAction, EKeys::Mouse2D, false, false, false);
	Map(LookStickAction, EKeys::Gamepad_Right2D, false, false, true);

	MapButton(FireAction, EKeys::LeftMouseButton);
	MapButton(FireAction, EKeys::Gamepad_RightTrigger);
	MapButton(AimAction, EKeys::RightMouseButton);
	MapButton(AimAction, EKeys::Gamepad_LeftTrigger);
	MapButton(SprintAction, EKeys::LeftShift);
	MapButton(SprintToggleAction, EKeys::Gamepad_LeftThumbstick);
	MapButton(JumpAction, EKeys::SpaceBar);
	MapButton(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	MapButton(CrouchAction, EKeys::LeftControl);
	MapButton(CrouchAction, EKeys::C);
	MapButton(CrouchAction, EKeys::Gamepad_FaceButton_Right);
	MapButton(ReloadAction, EKeys::R);
	MapButton(ReloadAction, EKeys::Gamepad_FaceButton_Left);
	MapButton(ShoulderAction, EKeys::V);
	MapButton(ShoulderAction, EKeys::Gamepad_RightThumbstick);
	MapButton(HelpAction, EKeys::H);
}

void AHeavyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildInput();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(InputContext, 0);
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogHeavy, Error, TEXT("HeavyCharacter needs Enhanced Input. Check DefaultInputComponentClass in Config/DefaultInput.ini."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHeavyCharacter::Input_Move);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AHeavyCharacter::Input_MoveReleased);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHeavyCharacter::Input_Look);
	Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AHeavyCharacter::Input_LookStick);
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_FirePressed);
	Input->BindAction(FireAction, ETriggerEvent::Completed, this, &AHeavyCharacter::Input_FireReleased);
	Input->BindAction(AimAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_AimPressed);
	Input->BindAction(AimAction, ETriggerEvent::Completed, this, &AHeavyCharacter::Input_AimReleased);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_SprintPressed);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHeavyCharacter::Input_SprintReleased);
	Input->BindAction(SprintToggleAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_SprintToggle);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_JumpPressed);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHeavyCharacter::Input_JumpReleased);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_Crouch);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_Reload);
	Input->BindAction(ShoulderAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_SwapShoulder);
	Input->BindAction(HelpAction, ETriggerEvent::Started, this, &AHeavyCharacter::Input_ToggleHelp);
}

void AHeavyCharacter::Input_Move(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();

	// Applied here (pre-physics) rather than in Tick (post-physics) so movement has no frame of delay.
	const float ViewYaw = static_cast<float>(GetLookRotation().Yaw);
	const FRotator YawRotation(0.f, ViewYaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	const float ForwardInput = static_cast<float>(MoveInput.Y);
	float RightInput = static_cast<float>(MoveInput.X);
	if (bIsSprinting)
	{
		RightInput *= SprintStrafeScale;
	}

	AddMovementInput(Forward, ForwardInput);
	AddMovementInput(Right, RightInput);
}

void AHeavyCharacter::Input_MoveReleased()
{
	MoveInput = FVector2D::ZeroVector;
}

void AHeavyCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const float Scale = MouseSensitivity * HeavyMath::Lerp(1.f, AimLookScale, AimAlpha.Value);
	const float InvertScale = bInvertLookY ? -1.f : 1.f;
	AddViewRotation(static_cast<float>(Axis.X) * Scale, static_cast<float>(Axis.Y) * Scale * InvertScale);
}

void AHeavyCharacter::Input_LookStick(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const float DeltaTime = GetWorld()->GetDeltaSeconds();
	const float Scale = HeavyMath::Lerp(1.f, AimLookScale, AimAlpha.Value) * DeltaTime;
	const float InvertScale = bInvertLookY ? -1.f : 1.f;

	// Squared response curve for fine aim near the centre of the stick.
	const float X = static_cast<float>(Axis.X);
	const float Y = static_cast<float>(Axis.Y);
	const float CurvedX = X * FMath::Abs(X);
	const float CurvedY = Y * FMath::Abs(Y);
	AddViewRotation(CurvedX * static_cast<float>(GamepadLookRate.X) * Scale,
		CurvedY * static_cast<float>(GamepadLookRate.Y) * Scale * InvertScale);
}

void AHeavyCharacter::AddViewRotation(float YawDelta, float PitchDelta)
{
	if (!Controller)
	{
		return;
	}

	FRotator Rotation = Controller->GetControlRotation();
	const float Yaw = HeavyMath::NormalizeAngle(static_cast<float>(Rotation.Yaw) + YawDelta);
	const float Pitch = FMath::Clamp(HeavyMath::NormalizeAngle(static_cast<float>(Rotation.Pitch)) + PitchDelta, MinLookPitch, MaxLookPitch);
	Rotation = FRotator(Pitch, Yaw, 0.f);
	Controller->SetControlRotation(Rotation);
}

FRotator AHeavyCharacter::GetLookRotation() const
{
	return Controller ? Controller->GetControlRotation() : GetActorRotation();
}

void AHeavyCharacter::Input_FirePressed()
{
	bFireHeld = true;
	FireBufferedUntil = GetGameTime() + 0.3f;
	bSprintToggled = false;
}

void AHeavyCharacter::Input_FireReleased()
{
	bFireHeld = false;
}

void AHeavyCharacter::Input_AimPressed()
{
	bAimHeld = true;
	bSprintToggled = false;
}

void AHeavyCharacter::Input_AimReleased()
{
	bAimHeld = false;
}

void AHeavyCharacter::Input_SprintPressed()
{
	bSprintHeld = true;
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

void AHeavyCharacter::Input_SprintReleased()
{
	bSprintHeld = false;
}

void AHeavyCharacter::Input_SprintToggle()
{
	bSprintToggled = !bSprintToggled;
	if (bSprintToggled && bIsCrouched)
	{
		UnCrouch();
	}
}

void AHeavyCharacter::Input_JumpPressed()
{
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}
	Jump();
}

void AHeavyCharacter::Input_JumpReleased()
{
	StopJumping();
}

void AHeavyCharacter::Input_Crouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		bSprintToggled = false;
		Crouch();
	}
}

void AHeavyCharacter::Input_Reload()
{
	if (Weapon && Weapon->StartReload())
	{
		bSprintToggled = false;
	}
}

void AHeavyCharacter::Input_SwapShoulder()
{
	ShoulderSign = -ShoulderSign;
}

void AHeavyCharacter::Input_ToggleHelp()
{
	bShowHelp = !bShowHelp;
}

// =========================================================================================
// Per-frame
// =========================================================================================

float AHeavyCharacter::GetGameTime() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;
}

void AHeavyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Clamp hitches so the springs never explode.
	const float DeltaTime = FMath::Min(DeltaSeconds, 0.05f);
	if (DeltaTime <= 0.f)
	{
		return;
	}

	UpdateMovementState(DeltaTime);
	UpdateBodyAndAim(DeltaTime);
	UpdateAimPoint();
	UpdateTorsoAndLegs(DeltaTime);
	UpdateWeaponPose(DeltaTime);
	UpdateFiring();
	if (Weapon)
	{
		Weapon->UpdateWeapon(DeltaTime);
	}
	UpdateArms();
	UpdateCamera(DeltaTime);

	const float Now = GetGameTime();
	HitEvents.RemoveAll([Now](const FHeavyHitEvent& Event) { return Now - Event.Time > 1.2f; });
}

void AHeavyCharacter::UpdateMovementState(float DeltaTime)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const FVector Velocity = GetVelocity();
	const float Speed2D = static_cast<float>(Velocity.Size2D());
	const bool bGrounded = Move->IsMovingOnGround();
	const float Now = GetGameTime();

	// Filtered acceleration drives leaning and weapon sway.
	const FVector RawAcceleration = (Velocity - LastVelocity) / DeltaTime;
	LastVelocity = Velocity;
	SmoothedAcceleration = FMath::Lerp(SmoothedAcceleration, RawAcceleration, SmoothingAlpha(10.f, DeltaTime));

	if (Move->IsFalling())
	{
		LastAirVelocityZ = static_cast<float>(Velocity.Z);
	}

	// Sprint: forward only, not while aiming, crouched, firing or reloading.
	const bool bWantsSprint = bSprintHeld || bSprintToggled;
	const bool bMovingForward = MoveInput.Y > 0.4f;
	const bool bRecentlyFired = (Now - LastFireTime) < 0.35f;
	const bool bReloading = Weapon && Weapon->IsReloading();
	bIsSprinting = bWantsSprint && bMovingForward && !bAimHeld && !bIsCrouched && !bFireHeld && !bRecentlyFired && !bReloading;
	if (!bMovingForward)
	{
		bSprintToggled = false;
	}

	float TargetSpeed = JogSpeed;
	float Acceleration = JogAcceleration;
	if (bIsSprinting)
	{
		TargetSpeed = SprintSpeed;
		Acceleration = SprintAcceleration;
	}
	else if (bAimHeld)
	{
		TargetSpeed = AimWalkSpeed;
		Acceleration = AimAcceleration;
	}

	LandingPenalty = FMath::Max(0.f, LandingPenalty - DeltaTime / FMath::Max(LandingRecoveryTime, 0.01f));
	const float PenaltyScale = 1.f - LandingPenalty * LandingSpeedPenalty;
	Move->MaxWalkSpeed = TargetSpeed * PenaltyScale;
	Move->MaxWalkSpeedCrouched = (bAimHeld ? CrouchAimSpeed : CrouchSpeed) * PenaltyScale;
	Move->MaxAcceleration = Acceleration;

	// Pose blends. Critically damped so they ease in and out without overshoot.
	AimAlpha.Update((bAimHeld && !bIsSprinting) ? 1.f : 0.f, DeltaTime, 3.2f, 1.f);
	SprintAlpha.Update(bIsSprinting ? 1.f : 0.f, DeltaTime, 2.4f, 1.f);
	CrouchAlpha.Update(bIsCrouched ? 1.f : 0.f, DeltaTime, 3.f, 1.f);
	ReloadAlpha.Update((Weapon && Weapon->WantsReloadPose()) ? 1.f : 0.f, DeltaTime, 3.5f, 1.f);
	AirAlpha.Update(bGrounded ? 0.f : 1.f, DeltaTime, 3.f, 1.f);
	ShoulderSide.Update(ShoulderSign, DeltaTime, 2.2f, 1.f);

	SpeedAlpha = FMath::Clamp(Speed2D / FMath::Max(JogSpeed, 1.f), 0.f, 1.6f);

	// Stride phase: half a cycle (PI) per step. Turning on the spot shuffles the feet too.
	if (bGrounded)
	{
		const float StrideLength = HeavyMath::Lerp(115.f, 165.f, SprintAlpha.Value) * HeavyMath::Lerp(1.f, 0.75f, CrouchAlpha.Value);
		const float TurnShuffle = FMath::Abs(BodyYaw.Velocity) * 0.6f;
		const float StepsBefore = FMath::FloorToFloat(StridePhase / PI);
		StridePhase += (Speed2D + TurnShuffle) * DeltaTime / StrideLength * PI;
		const float StepsAfter = FMath::FloorToFloat(StridePhase / PI);
		if (StepsAfter != StepsBefore && bIsSprinting)
		{
			AddTrauma(0.06f); // Heavy footfalls.
		}
		if (StridePhase > 2.f * PI)
		{
			StridePhase -= 2.f * PI;
		}
	}

	SpreadBloom = FMath::Max(0.f, SpreadBloom - SpreadBloomRecovery * DeltaTime);
	Trauma = FMath::Max(0.f, Trauma - 1.6f * DeltaTime);
}

void AHeavyCharacter::UpdateBodyAndAim(float DeltaTime)
{
	const FRotator View = GetLookRotation();
	const float ViewYaw = HeavyMath::NormalizeAngle(static_cast<float>(View.Yaw));
	const float ViewPitch = HeavyMath::NormalizeAngle(static_cast<float>(View.Pitch));

	// The body follows the camera with weight. While sprinting it swings round to face the run.
	float BodyTarget = ViewYaw;
	const FVector Velocity = GetVelocity();
	if (SprintAlpha.Value > 0.01f && Velocity.SizeSquared2D() > 100.f * 100.f)
	{
		const float VelocityYaw = static_cast<float>(Velocity.Rotation().Yaw);
		BodyTarget = ViewYaw + HeavyMath::DeltaAngle(ViewYaw, VelocityYaw) * HeavyMath::Clamp01(SprintAlpha.Value);
	}

	const float BodyFrequency = HeavyMath::Lerp(BodyTurnFrequency, BodyTurnFrequencyAim, AimAlpha.Value);
	BodyYaw.UpdateAngle(BodyTarget, DeltaTime, BodyFrequency, BodyTurnDamping);
	BodyYaw.ClampAngleLag(BodyTarget, BodyMaxLag);
	SetActorRotation(FRotator(0.f, BodyYaw.Value, 0.f));

	// The gun trails the camera and settles with a little overshoot. Heavier when aiming.
	const float Frequency = HeavyMath::Lerp(WeaponLagFrequencyHip, WeaponLagFrequencyAim, AimAlpha.Value);
	const float Damping = HeavyMath::Lerp(WeaponLagDampingHip, WeaponLagDampingAim, AimAlpha.Value);
	const float MaxLag = HeavyMath::Lerp(WeaponMaxLagHip, WeaponMaxLagAim, AimAlpha.Value);
	AimYaw.UpdateAngle(ViewYaw, DeltaTime, Frequency, Damping);
	AimPitch.UpdateAngle(ViewPitch, DeltaTime, Frequency, Damping);
	AimYaw.ClampAngleLag(ViewYaw, MaxLag);
	AimPitch.ClampAngleLag(ViewPitch, MaxLag);
}

FVector AHeavyCharacter::TraceAim(const FRotator& Rotation, bool& bOutOnTarget) const
{
	bOutOnTarget = false;

	const FVector Direction = Rotation.Vector();
	const FVector CameraLocation = Camera->GetComponentLocation();
	const FVector PivotLocation = AimPivot->GetComponentLocation();

	// Start level with the character so nothing between the camera and the player gets hit.
	const float Along = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(PivotLocation - CameraLocation, Direction)));
	const FVector Start = CameraLocation + Direction * Along;
	const FVector End = Start + Direction * AimTraceRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HeavyAimTrace), false, this);
	if (Weapon)
	{
		Params.AddIgnoredActor(Weapon);
	}

	FVector Result = End;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		Result = Hit.ImpactPoint;
		const AHeavyTargetDummy* Dummy = Cast<AHeavyTargetDummy>(Hit.GetActor());
		bOutOnTarget = Dummy && !Dummy->IsDown();
	}

	// Keep the point far enough ahead that the gun never folds back on itself.
	constexpr float MinDistance = 150.f;
	if (FVector::DistSquared(Result, PivotLocation) < MinDistance * MinDistance)
	{
		Result = PivotLocation + Direction * MinDistance;
	}
	return Result;
}

void AHeavyCharacter::UpdateAimPoint()
{
	const FRotator WeaponRotation(AimPitch.Value, AimYaw.Value, 0.f);
	bool bWeaponOnTarget = false;
	VisualAimPoint = TraceAim(WeaponRotation, bWeaponOnTarget);

	if (bWeaponLagAffectsAim)
	{
		AimPoint = VisualAimPoint;
		bAimOnTarget = bWeaponOnTarget;
	}
	else
	{
		AimPoint = TraceAim(GetLookRotation(), bAimOnTarget);
	}
}

void AHeavyCharacter::UpdateTorsoAndLegs(float DeltaTime)
{
	const FRotator BodyRotation(0.f, BodyYaw.Value, 0.f);
	const FVector LocalVelocity = BodyRotation.UnrotateVector(GetVelocity());
	const FVector LocalAcceleration = BodyRotation.UnrotateVector(SmoothedAcceleration);
	const float Speed2D = static_cast<float>(LocalVelocity.Size2D());
	const float Grounded = 1.f - HeavyMath::Clamp01(AirAlpha.Value);
	const float CrouchBlend = HeavyMath::Clamp01(CrouchAlpha.Value);
	const float Sprint = HeavyMath::Clamp01(SprintAlpha.Value);

	// Lean into acceleration and into turns. Under-damped, so the torso rocks back when you stop.
	const float AccelForward = FMath::Clamp(static_cast<float>(LocalAcceleration.X) / 1400.f, -1.f, 1.f);
	const float AccelRight = FMath::Clamp(static_cast<float>(LocalAcceleration.Y) / 1400.f, -1.f, 1.f);
	const float TurnLean = FMath::Clamp(BodyYaw.Velocity * 0.02f, -6.f, 6.f);
	LeanForward.Update(AccelForward * 7.f + Sprint * 9.f, DeltaTime, 2.2f, 0.45f);
	LeanRight.Update(AccelRight * 5.f + TurnLean, DeltaTime, 2.2f, 0.5f);

	// Hips: crouch drop, footstep bob and the landing spring.
	HipDrop.Update(0.f, DeltaTime, 3.5f, 0.45f);
	const float Bob = -FMath::Abs(FMath::Sin(StridePhase)) * 3.5f * FMath::Min(SpeedAlpha, 1.3f) * Grounded;
	const float PelvisTwist = FMath::Sin(StridePhase) * 5.f * FMath::Min(SpeedAlpha, 1.f) * Grounded;
	Pelvis->SetRelativeLocation(FVector(0.f, 0.f, StandingHipHeight - CrouchHipDrop * CrouchBlend + Bob + HipDrop.Value));
	Pelvis->SetRelativeRotation(FRotator(0.f, PelvisTwist, 0.f));

	// Spine: lean, twist towards the gun, and blade the shoulders side-on when holding it up.
	const float AimRelativeYaw = HeavyMath::DeltaAngle(BodyYaw.Value, AimYaw.Value);
	const float Blade = HeavyMath::Lerp(BladeAngleHip, BladeAngleAim, AimAlpha.Value) * (1.f - Sprint);
	const float SpineYaw = FMath::Clamp(AimRelativeYaw * 0.55f, -40.f, 40.f) + Blade - PelvisTwist;
	const float SpinePitch = -LeanForward.Value - 12.f * CrouchBlend; // Negative pitch leans forward.
	Spine->SetRelativeRotation(FRotator(SpinePitch, SpineYaw, LeanRight.Value));

	// Head tracks the aim.
	const float HeadYaw = FMath::Clamp(AimRelativeYaw - SpineYaw, -60.f, 60.f);
	const float HeadPitch = FMath::Clamp(AimPitch.Value * 0.7f - SpinePitch, -45.f, 45.f);
	Neck->SetRelativeRotation(FRotator(HeadPitch, HeadYaw, -LeanRight.Value * 0.5f));

	// Legs: a simple walk cycle along the actual direction of travel, bent for crouch and air.
	const float SwingAmount = HeavyMath::Lerp(26.f, 38.f, Sprint) * FMath::Min(SpeedAlpha, 1.f) * Grounded * HeavyMath::Lerp(1.f, 0.6f, CrouchBlend);
	const float MoveForward = Speed2D > 10.f ? static_cast<float>(LocalVelocity.X) / Speed2D : 0.f;
	const float MoveRight = Speed2D > 10.f ? static_cast<float>(LocalVelocity.Y) / Speed2D : 0.f;
	const float StepDirection = MoveForward >= -0.3f ? 1.f : -1.f;
	const float Air = HeavyMath::Clamp01(AirAlpha.Value);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bLeft = (Side == 0);
		const float Phase = StridePhase + (bLeft ? 0.f : PI);
		const float Swing = FMath::Sin(Phase) * SwingAmount;

		// Positive pitch swings the foot forward; positive roll swings it to the left.
		float ThighPitch = Swing * MoveForward + CrouchBlend * 50.f + Air * 22.f;
		float ThighRoll = -Swing * 0.6f * MoveRight + (bLeft ? 6.f : -6.f) * CrouchBlend;
		const float KneeLift = FMath::Max(0.f, FMath::Cos(Phase) * StepDirection) * SwingAmount * 1.3f;
		const float KneePitch = -(KneeLift + CrouchBlend * 100.f + Air * 45.f);
		// Keep the boot roughly flat on the ground.
		const float AnklePitch = -(ThighPitch + KneePitch) * 0.85f;

		USceneComponent* Thigh = bLeft ? ThighL : ThighR;
		USceneComponent* Knee = bLeft ? KneeL : KneeR;
		USceneComponent* Ankle = bLeft ? AnkleL : AnkleR;
		Thigh->SetRelativeRotation(FRotator(ThighPitch, 0.f, ThighRoll));
		Knee->SetRelativeRotation(FRotator(KneePitch, 0.f, 0.f));
		Ankle->SetRelativeRotation(FRotator(AnklePitch, 0.f, -ThighRoll));
	}
}

void AHeavyCharacter::UpdateWeaponPose(float DeltaTime)
{
	const float Aim = HeavyMath::Clamp01(AimAlpha.Value);
	const float Sprint = HeavyMath::Clamp01(SprintAlpha.Value);
	const float Reload = HeavyMath::Clamp01(ReloadAlpha.Value);
	const float Grounded = 1.f - HeavyMath::Clamp01(AirAlpha.Value);
	const float Now = GetGameTime();

	// Turn the chest pivot to face the (lagging) aim point.
	const FVector PivotLocation = AimPivot->GetComponentLocation();
	AimPivot->SetWorldRotation((VisualAimPoint - PivotLocation).Rotation());

	// Movement sway: the gun shifts against changes in velocity and bobs with the stride.
	const FVector PivotAcceleration = AimPivot->GetComponentTransform().InverseTransformVectorNoScale(SmoothedAcceleration);
	FVector SwayTarget = -PivotAcceleration * (0.0028f * MovementSway);
	SwayTarget.X = FMath::Clamp(static_cast<float>(SwayTarget.X), -4.f, 4.f);
	SwayTarget.Y = FMath::Clamp(static_cast<float>(SwayTarget.Y), -4.f, 4.f);
	SwayTarget.Z = FMath::Clamp(static_cast<float>(SwayTarget.Z), -4.f, 4.f);
	const float StrideBob = FMath::Min(SpeedAlpha, 1.3f) * Grounded * HeavyMath::Lerp(1.f, 0.4f, Aim);
	SwayTarget.Z += FMath::Sin(StridePhase * 2.f) * 0.7f * StrideBob;
	SwayTarget.Y += FMath::Sin(StridePhase) * 0.9f * StrideBob;

	WeaponSway.Update(SwayTarget, DeltaTime, 3.2f, 0.45f);
	WeaponKick.Update(FVector::ZeroVector, DeltaTime, 8.f, 0.42f);
	MuzzleFlip.Update(0.f, DeltaTime, 6.5f, 0.42f);
	KickRoll.Update(0.f, DeltaTime, 5.f, 0.4f);

	// Blend the held positions.
	FVector Offset = FMath::Lerp(HipWeaponOffset, AimWeaponOffset, Aim);
	Offset = FMath::Lerp(Offset, SprintWeaponOffset, Sprint);
	Offset += ReloadWeaponOffset * Reload;
	Offset += WeaponSway.Get() + WeaponKick.Get();
	WeaponMount->SetRelativeLocation(Offset);

	// The barrel points at the aim point; sprinting blends to a low carry along the body.
	const FVector MountLocation = WeaponMount->GetComponentLocation();
	const FQuat AimQuat = (VisualAimPoint - MountLocation).Rotation().Quaternion();
	const FQuat CarryQuat = FRotator(-5.f, BodyYaw.Value, 0.f).Quaternion();
	const FQuat BaseQuat = FQuat::Slerp(AimQuat, CarryQuat, Sprint);

	// Cant the gun against fast turns, plus a slow breathing drift.
	const float TurnRoll = FMath::Clamp(-AimYaw.Velocity * WeaponTurnRoll, -14.f, 14.f);
	const float Breathing = HeavyMath::Lerp(1.f, 0.6f, Aim);
	const float BreathPitch = FMath::Sin(Now * 1.1f) * 0.35f * Breathing;
	const float BreathYaw = FMath::Sin(Now * 0.7f + 1.3f) * 0.4f * Breathing;

	FRotator Additive = SprintWeaponRotation * Sprint + ReloadWeaponRotation * Reload;
	Additive += FRotator(MuzzleFlip.Value + BreathPitch, BreathYaw, TurnRoll + KickRoll.Value + LeanRight.Value * 0.4f);
	WeaponMount->SetWorldRotation(BaseQuat * Additive.Quaternion());
}

void AHeavyCharacter::UpdateFiring()
{
	if (!Weapon)
	{
		return;
	}

	const float Now = GetGameTime();
	const bool bWantsFire = bFireHeld || Now < FireBufferedUntil;
	if (!bWantsFire)
	{
		return;
	}

	// The gun has to come up out of the sprint carry first: that's the sprint-to-fire delay.
	if (SprintAlpha.Value > 0.3f || Weapon->IsCycling())
	{
		return;
	}

	// An empty gun dry-fires and starts reloading on its own.
	const bool bWillDryFire = Weapon->GetShells() == 0 && !Weapon->IsReloading();

	TArray<FShotgunHitReport> Hits;
	if (Weapon->Fire(AimPoint, GetCurrentSpread(), Hits))
	{
		FireBufferedUntil = -1.f;
		LastFireTime = Now;
		ApplyRecoil();
		RecordHits(Hits);
	}
	else if (bWillDryFire)
	{
		// Make the player press again; a held trigger would otherwise cut the auto-reload
		// short after every shell.
		bFireHeld = false;
		FireBufferedUntil = -1.f;
	}
}

void AHeavyCharacter::ApplyRecoil()
{
	const float Scale = HeavyMath::Lerp(1.f, AimRecoilScale, HeavyMath::Clamp01(AimAlpha.Value)) * (bIsCrouched ? 0.85f : 1.f);

	// A small permanent kick of the view that the player pulls back down...
	AddViewRotation(FMath::FRandRange(-1.f, 1.f) * RecoilViewYawJitter * Scale, RecoilViewKick * Scale);

	// ...and everything else as impulses into the springs, so it settles naturally.
	AimPitch.AddImpulse(RecoilAimKick * Scale);
	AimYaw.AddImpulse(FMath::FRandRange(-0.35f, 0.35f) * RecoilAimKick * Scale);
	MuzzleFlip.AddImpulse(RecoilMuzzleFlip * Scale);
	KickRoll.AddImpulse(FMath::FRandRange(-90.f, 90.f) * Scale);
	WeaponKick.AddImpulse(FVector(-RecoilKickBack, FMath::FRandRange(-40.f, 40.f), RecoilKickBack * 0.25f) * Scale);
	LeanForward.AddImpulse(-RecoilBodyRock * Scale);
	CameraPunch.AddImpulse(RecoilCameraPunch * Scale);
	AddTrauma(RecoilShake * Scale);
	SpreadBloom += SpreadBloomPerShot;
}

void AHeavyCharacter::RecordHits(const TArray<FShotgunHitReport>& Hits)
{
	if (Hits.Num() == 0)
	{
		return;
	}

	const float Now = GetGameTime();
	bLastHitKill = false;
	for (const FShotgunHitReport& Report : Hits)
	{
		FHeavyHitEvent& Event = HitEvents.AddDefaulted_GetRef();
		Event.Location = Report.Location;
		Event.Damage = Report.Damage;
		Event.Time = Now;
		Event.bCritical = Report.bCritical;
		Event.bKill = Report.bKilled;
		bLastHitKill |= Report.bKilled;
	}
	LastHitTime = Now;
}

void AHeavyCharacter::UpdateArms()
{
	if (!Weapon)
	{
		return;
	}

	const FVector RightHand = Weapon->GetTriggerHandLocation();
	FVector LeftHand = Weapon->GetPumpHandLocation();
	bool bShowShell = false;

	// Reloading: the support hand fetches a shell from the belt and feeds it into the loading port.
	const float Reload = HeavyMath::Clamp01(ReloadAlpha.Value);
	if (Reload > 0.01f)
	{
		const FVector Pouch = ReloadPouch->GetComponentLocation();
		const FVector Port = Weapon->GetLoadingPortLocation();
		const float Insert = Weapon->GetInsertPhase();

		FVector ReloadHand = Port;
		if (Insert >= 0.f)
		{
			if (Insert < 0.4f)
			{
				ReloadHand = FMath::Lerp(Port, Pouch, HeavyMath::Ease(Insert / 0.4f));
			}
			else if (Insert < 0.75f)
			{
				ReloadHand = FMath::Lerp(Pouch, Port, HeavyMath::Ease((Insert - 0.4f) / 0.35f));
				bShowShell = true;
			}
			else
			{
				// Thumb the shell up into the tube.
				const float Push = FMath::Sin((Insert - 0.75f) / 0.25f * PI);
				ReloadHand = Port + Weapon->GetActorForwardVector() * (4.f * Push) + Weapon->GetActorUpVector() * (1.5f * Push);
				bShowShell = Insert < 0.85f;
			}
		}
		LeftHand = FMath::Lerp(LeftHand, ReloadHand, Reload);
	}

	const FVector BodyForward = Spine->GetForwardVector();
	const FVector BodyRight = Spine->GetRightVector();
	const FVector BodyUp = Spine->GetUpVector();

	// Right elbow out and down, left elbow mostly down under the pump.
	const FVector RightShoulder = ShoulderR->GetComponentLocation();
	const FVector LeftShoulder = ShoulderL->GetComponentLocation();
	const FVector RightElbow = SolveElbow(RightShoulder, RightHand, UpperArmLength, ForearmLength, BodyRight * 0.9f - BodyUp * 0.6f - BodyForward * 0.2f);
	const FVector LeftElbow = SolveElbow(LeftShoulder, LeftHand, UpperArmLength, ForearmLength, -BodyRight * 0.4f - BodyUp * 1.f);

	PlaceSegment(UpperArmR, RightShoulder, RightElbow, 11.f);
	PlaceSegment(ForearmR, RightElbow, RightHand, 10.f);
	PlaceSegment(UpperArmL, LeftShoulder, LeftElbow, 11.f);
	PlaceSegment(ForearmL, LeftElbow, LeftHand, 10.f);
	ElbowR->SetWorldLocation(RightElbow);
	ElbowL->SetWorldLocation(LeftElbow);

	const FRotator GunRotation = Weapon->GetActorRotation();
	HandR->SetWorldLocationAndRotation(RightHand, GunRotation);
	HandL->SetWorldLocationAndRotation(LeftHand, GunRotation);

	HandShell->SetVisibility(bShowShell);
	if (bShowShell)
	{
		HandShell->SetWorldLocationAndRotation(LeftHand + Weapon->GetActorUpVector() * 3.f,
			(Weapon->GetActorRotation().Quaternion() * FRotator(90.f, 0.f, 0.f).Quaternion()).Rotator());
	}
}

void AHeavyCharacter::UpdateCamera(float DeltaTime)
{
	const float Aim = HeavyMath::Clamp01(AimAlpha.Value);
	const float Sprint = HeavyMath::Clamp01(SprintAlpha.Value);
	const float Grounded = 1.f - HeavyMath::Clamp01(AirAlpha.Value);

	float ArmLength = HeavyMath::Lerp(HipArmLength, AimArmLength, Aim);
	ArmLength = HeavyMath::Lerp(ArmLength, SprintArmLength, Sprint);
	SpringArm->TargetArmLength = ArmLength;

	FVector SocketOffset = FMath::Lerp(HipCameraOffset, AimCameraOffset, Aim);
	SocketOffset.Y *= ShoulderSide.Value;
	SpringArm->SocketOffset = SocketOffset;
	SpringArm->CameraLagSpeed = HeavyMath::Lerp(CameraLagSpeedHip, CameraLagSpeedAim, Aim);

	Camera->SetFieldOfView(HeavyMath::Lerp(HipFOV, AimFOV, Aim) + SprintFOVBoost * Sprint);

	// Procedural camera on top of the boom: footstep bob, landing dip, recoil punch,
	// a slight roll into strafes, and trauma-based shake.
	const float BobAmount = CameraBobScale * FMath::Min(SpeedAlpha, 1.4f) * Grounded * HeavyMath::Lerp(1.f, 0.35f, Aim) * HeavyMath::Lerp(1.f, 1.6f, Sprint);
	CameraDip.Update(0.f, DeltaTime, 3.f, 0.5f);
	CameraPunch.Update(0.f, DeltaTime, 5.5f, 0.5f);

	const FRotator ViewYawOnly(0.f, GetLookRotation().Yaw, 0.f);
	const float StrafeSpeed = static_cast<float>(ViewYawOnly.UnrotateVector(GetVelocity()).Y);
	CameraRoll.Update(FMath::Clamp(StrafeSpeed / FMath::Max(JogSpeed, 1.f), -1.f, 1.f) * 0.7f, DeltaTime, 2.f, 1.f);

	const FVector CameraOffset(
		0.f,
		FMath::Sin(StridePhase) * 0.6f * BobAmount,
		-FMath::Abs(FMath::Sin(StridePhase)) * 1.4f * BobAmount + CameraDip.Value);

	const float Shake = Trauma * Trauma;
	const float NoiseTime = GetGameTime() * 22.f;
	const FRotator ShakeRotation(
		3.0f * Shake * FMath::PerlinNoise1D(NoiseTime),
		2.0f * Shake * FMath::PerlinNoise1D(NoiseTime + 37.1f),
		2.5f * Shake * FMath::PerlinNoise1D(NoiseTime + 71.3f));

	Camera->SetRelativeLocationAndRotation(CameraOffset, FRotator(CameraPunch.Value, 0.f, CameraRoll.Value) + ShakeRotation);
}

void AHeavyCharacter::AddTrauma(float Amount)
{
	Trauma = FMath::Clamp(Trauma + Amount, 0.f, 1.f);
}

// =========================================================================================
// Events
// =========================================================================================

void AHeavyCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	const float ImpactSpeed = FMath::Max(0.f, -LastAirVelocityZ);
	const float Severity = HeavyMath::MapClamped(300.f, 1300.f, 0.f, 1.f, ImpactSpeed);
	LastAirVelocityZ = 0.f;
	if (Severity <= 0.f)
	{
		return;
	}

	// The body soaks up the landing, the gun dips, the camera follows a beat later.
	HipDrop.AddImpulse(-160.f * Severity);
	LeanForward.AddImpulse(60.f * Severity);
	WeaponKick.AddImpulse(FVector(0.f, 0.f, -140.f * Severity));
	CameraDip.AddImpulse(-120.f * Severity);
	CameraPunch.AddImpulse(-25.f * Severity);
	AddTrauma(0.35f * Severity);
	LandingPenalty = FMath::Max(LandingPenalty, Severity);
}

void AHeavyCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	// Push-off: a quick dip and the gun lagging behind the jump.
	HipDrop.AddImpulse(-50.f);
	WeaponKick.AddImpulse(FVector(0.f, 0.f, -60.f));
	CameraDip.AddImpulse(-30.f);
}

float AHeavyCharacter::GetWeaponLagDegrees() const
{
	const FVector ViewDirection = GetLookRotation().Vector();
	const FVector WeaponDirection = FRotator(AimPitch.Value, AimYaw.Value, 0.f).Vector();
	const float Dot = FMath::Clamp(static_cast<float>(FVector::DotProduct(ViewDirection, WeaponDirection)), -1.f, 1.f);
	return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

float AHeavyCharacter::GetCurrentSpread() const
{
	float Spread = HeavyMath::Lerp(HipSpread, AimSpread, HeavyMath::Clamp01(AimAlpha.Value));
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	Spread += MovingSpreadAdd * HeavyMath::Clamp01(Speed / FMath::Max(JogSpeed, 1.f));
	if (!GetCharacterMovement()->IsMovingOnGround())
	{
		Spread += AirborneSpreadAdd;
	}
	if (bIsCrouched)
	{
		Spread *= CrouchSpreadScale;
	}
	return Spread + SpreadBloom;
}
