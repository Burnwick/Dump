#include "PFCharacter.h"
#include "PFShotgun.h"
#include "PFTarget.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
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

APFCharacter::APFCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->bUseControllerDesiredRotation = true;
	Move->RotationRate = FRotator(0.f, 900.f, 0.f);
	Move->MaxWalkSpeed = RunSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->JumpZVelocity = 520.f;
	Move->AirControl = 0.35f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(62.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = HipArmLength;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->ProbeSize = 12.f;
	CameraBoom->SocketOffset = FVector(0.f, HipShoulderOffset, 10.f);
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight - 92.f));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(HipFOV);

	// No skeletal mesh: the body is assembled from primitives in BuildBody()
	GetMesh()->SetVisibility(false);
}

void APFCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BuildBody();
}

void APFCharacter::BeginPlay()
{
	Super::BeginPlay();

	CameraHeight = StandingCameraHeight;
	AimPoint = GetActorLocation() + GetActorForwardVector() * 1000.f;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Weapon = GetWorld()->SpawnActor<APFShotgun>(APFShotgun::StaticClass(), GetActorTransform(), Params);
	if (Weapon && BodyRoot)
	{
		Weapon->AttachToComponent(BodyRoot, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void APFCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Weapon)
	{
		Weapon->Destroy();
		Weapon = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void APFCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCombat(DeltaSeconds);
	UpdateMovementState(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
	UpdateAimPoint();
	UpdateBody(DeltaSeconds);

	if (Weapon)
	{
		Weapon->SetPumpAlpha(GetPumpAlpha());
		Weapon->TickWeapon(DeltaSeconds);
	}
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

UInputAction* APFCharacter::MakeAction(const TCHAR* Name, bool bAxis2D)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = bAxis2D ? EInputActionValueType::Axis2D : EInputActionValueType::Boolean;
	return Action;
}

void APFCharacter::MapKey(UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate, bool bNegateYOnly)
{
	FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(Action, Key);
	if (bSwizzle)
	{
		// Turns a 1D key press (X) into the forward axis (Y)
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MappingContext);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
	if (bNegate || bNegateYOnly)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(MappingContext);
		if (bNegateYOnly)
		{
			Negate->bX = false;
			Negate->bZ = false;
		}
		Mapping.Modifiers.Add(Negate);
	}
}

void APFCharacter::CreateInput()
{
	if (MappingContext)
	{
		return;
	}

	MoveAction = MakeAction(TEXT("IA_Move"), true);
	LookAction = MakeAction(TEXT("IA_Look"), true);
	JumpAction = MakeAction(TEXT("IA_Jump"), false);
	FireAction = MakeAction(TEXT("IA_Fire"), false);
	AimAction = MakeAction(TEXT("IA_Aim"), false);
	SprintAction = MakeAction(TEXT("IA_Sprint"), false);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), false);
	ReloadAction = MakeAction(TEXT("IA_Reload"), false);
	ShoulderAction = MakeAction(TEXT("IA_ShoulderSwap"), false);
	HolsterAction = MakeAction(TEXT("IA_Holster"), false);
	MeleeAction = MakeAction(TEXT("IA_Melee"), false);
	ResetAction = MakeAction(TEXT("IA_ResetRange"), false);
	HelpAction = MakeAction(TEXT("IA_Help"), false);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_PrimFront"));

	// WASD -> 2D move axis (X = right, Y = forward)
	MapKey(MoveAction, EKeys::W, true);
	MapKey(MoveAction, EKeys::S, true, true);
	MapKey(MoveAction, EKeys::D);
	MapKey(MoveAction, EKeys::A, false, true);

	// Mouse look (Y negated, same as the engine's third person template)
	MapKey(LookAction, EKeys::Mouse2D, false, false, true);

	MapKey(JumpAction, EKeys::SpaceBar);
	MapKey(FireAction, EKeys::LeftMouseButton);
	MapKey(AimAction, EKeys::RightMouseButton);
	MapKey(SprintAction, EKeys::LeftShift);
	MapKey(CrouchAction, EKeys::C);
	MapKey(CrouchAction, EKeys::LeftControl);
	MapKey(ReloadAction, EKeys::R);
	MapKey(ShoulderAction, EKeys::Q);
	MapKey(HolsterAction, EKeys::H);
	MapKey(HolsterAction, EKeys::One);
	MapKey(MeleeAction, EKeys::V);
	MapKey(MeleeAction, EKeys::F);
	MapKey(MeleeAction, EKeys::ThumbMouseButton);
	MapKey(ResetAction, EKeys::T);
	MapKey(HelpAction, EKeys::F1);
}

void APFCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	CreateInput();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("PrimFront needs Enhanced Input (see Config/DefaultInput.ini)."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APFCharacter::OnMove);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &APFCharacter::OnMoveStop);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &APFCharacter::OnLook);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &APFCharacter::OnJumpPressed);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &APFCharacter::OnJumpReleased);
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &APFCharacter::OnFirePressed);
	Input->BindAction(AimAction, ETriggerEvent::Started, this, &APFCharacter::OnAimPressed);
	Input->BindAction(AimAction, ETriggerEvent::Completed, this, &APFCharacter::OnAimReleased);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &APFCharacter::OnSprintPressed);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &APFCharacter::OnSprintReleased);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &APFCharacter::OnCrouchPressed);
	Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &APFCharacter::OnReloadPressed);
	Input->BindAction(ShoulderAction, ETriggerEvent::Started, this, &APFCharacter::OnShoulderSwapPressed);
	Input->BindAction(HolsterAction, ETriggerEvent::Started, this, &APFCharacter::OnHolsterPressed);
	Input->BindAction(MeleeAction, ETriggerEvent::Started, this, &APFCharacter::OnMeleePressed);
	Input->BindAction(ResetAction, ETriggerEvent::Started, this, &APFCharacter::OnResetRangePressed);
	Input->BindAction(HelpAction, ETriggerEvent::Started, this, &APFCharacter::OnHelpPressed);
}

void APFCharacter::OnMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), (float)MoveInput.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), (float)MoveInput.X);
}

void APFCharacter::OnMoveStop()
{
	MoveInput = FVector2D::ZeroVector;
}

void APFCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	const float Scale = LookSensitivity * FMath::Lerp(1.f, AdsSensitivityScale, AimAlpha);
	AddControllerYawInput((float)Look.X * Scale);
	AddControllerPitchInput((float)Look.Y * Scale);
}

void APFCharacter::OnJumpPressed()
{
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}
	Jump();
}

void APFCharacter::OnJumpReleased()
{
	StopJumping();
}

void APFCharacter::OnAimPressed()
{
	bAimHeld = true;
	if (bHolstered)
	{
		SetHolstered(false);
	}
}

void APFCharacter::OnAimReleased()
{
	bAimHeld = false;
}

void APFCharacter::OnSprintPressed()
{
	bSprintHeld = true;
	if (bIsCrouched)
	{
		UnCrouch();
	}
}

void APFCharacter::OnSprintReleased()
{
	bSprintHeld = false;
}

void APFCharacter::OnCrouchPressed()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		bSprintHeld = false;
		Crouch();
	}
}

void APFCharacter::OnReloadPressed()
{
	StartReload();
}

void APFCharacter::OnShoulderSwapPressed()
{
	ShoulderSide = -ShoulderSide;
}

void APFCharacter::OnHolsterPressed()
{
	SetHolstered(!bHolstered);
}

void APFCharacter::OnMeleePressed()
{
	StartMelee();
}

void APFCharacter::OnResetRangePressed()
{
	for (TActorIterator<APFTarget> It(GetWorld()); It; ++It)
	{
		It->ResetTarget();
	}
	if (Weapon)
	{
		Weapon->Refill();
	}
	CancelReload();
	ShotsFired = PelletsFired = PelletsHit = Kills = Headshots = MeleeHits = 0;
}

void APFCharacter::OnHelpPressed()
{
	bShowHelp = !bShowHelp;
}

// ---------------------------------------------------------------------------
// Movement state and camera
// ---------------------------------------------------------------------------

bool APFCharacter::CanAim() const
{
	return !bHolstered && HolsterAlpha < 0.5f && ReloadPhase == EPFReloadPhase::None && MeleeTimer <= 0.f;
}

void APFCharacter::UpdateMovementState(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float Speed2D = (float)GetVelocity().Size2D();
	const bool bWantsAim = bAimHeld && CanAim();

	// Armed: sprint only when pushing forward. Holstered: the body turns to the movement, so any direction works.
	const bool bMovingForward = bHolstered ? MoveInput.Size() > 0.3 : (MoveInput.Y > 0.5 && FMath::Abs(MoveInput.X) < 0.75);
	bSprinting = bSprintHeld && bMovingForward && !bWantsAim && !bIsCrouched && MeleeTimer <= 0.f && Speed2D > 50.f;

	if (bSprinting && ReloadPhase != EPFReloadPhase::None)
	{
		CancelReload();
	}

	AimAlpha = FMath::FInterpTo(AimAlpha, bWantsAim ? 1.f : 0.f, DeltaSeconds, 14.f);
	SprintAlpha = FMath::FInterpTo(SprintAlpha, bSprinting ? 1.f : 0.f, DeltaSeconds, 8.f);
	ShoulderAlpha = FMath::FInterpTo(ShoulderAlpha, ShoulderSide, DeltaSeconds, 10.f);

	Move->MaxWalkSpeed = bSprinting ? SprintSpeed : FMath::Lerp(RunSpeed, AimWalkSpeed, AimAlpha);
	Move->MaxWalkSpeedCrouched = FMath::Lerp(CrouchSpeed, CrouchSpeed * 0.7f, AimAlpha);

	// Weapon out: strafe and face the camera. Holstered: turn towards the direction of travel.
	const bool bStrafe = !bHolstered || HolsterAlpha < 0.99f || MeleeTimer > 0.f;
	Move->bOrientRotationToMovement = !bStrafe;
	Move->bUseControllerDesiredRotation = bStrafe;
	Move->RotationRate = FRotator(0.f, bStrafe ? 900.f : 600.f, 0.f);
}

void APFCharacter::UpdateCamera(float DeltaSeconds)
{
	// The boom is attached to the capsule, whose centre jumps when crouching; keep the camera
	// height measured from the feet and blend it so the view lowers smoothly.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	CameraHeight = FMath::FInterpTo(CameraHeight, bIsCrouched ? CrouchedCameraHeight : StandingCameraHeight, DeltaSeconds, 10.f);
	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CameraHeight - HalfHeight));

	float ArmLength = FMath::Lerp(HipArmLength, AdsArmLength, AimAlpha);
	ArmLength = FMath::Lerp(ArmLength, SprintArmLength, SprintAlpha);
	CameraBoom->TargetArmLength = ArmLength;

	const float Side = FMath::Lerp(HipShoulderOffset, AdsShoulderOffset, AimAlpha) * ShoulderAlpha;
	CameraBoom->SocketOffset = FVector(0.f, Side, FMath::Lerp(10.f, 4.f, AimAlpha));

	float FOV = FMath::Lerp(HipFOV, AdsFOV, AimAlpha);
	FOV = FMath::Lerp(FOV, SprintFOV, SprintAlpha);
	FollowCamera->SetFieldOfView(FOV);
}

float APFCharacter::GetCameraFOV() const
{
	return FollowCamera ? FollowCamera->FieldOfView : HipFOV;
}

void APFCharacter::UpdateAimPoint()
{
	const FVector CamLoc = FollowCamera->GetComponentLocation();
	const FVector CamDir = FollowCamera->GetForwardVector();

	// Start the trace level with the character so nothing between the camera and the player is hit
	const double Along = FMath::Max(FVector::DotProduct(GetActorLocation() - CamLoc, CamDir), 0.0);
	const FVector Start = CamLoc + CamDir * Along;
	const FVector End = CamLoc + CamDir * 20000.0;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PFAimPoint), false, this);
	if (Weapon)
	{
		Params.AddIgnoredActor(Weapon);
	}

	FHitResult Hit;
	FVector Point = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;

	// Very close aim points make the gun twist sideways; keep a minimum distance along the view
	if (FVector::DotProduct(Point - Start, CamDir) < 250.0)
	{
		Point = Start + CamDir * 250.0;
	}
	AimPoint = Point;
}
