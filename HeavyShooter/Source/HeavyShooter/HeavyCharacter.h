#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HeavySpring.h"
#include "HeavyCharacter.generated.h"

class AHeavyShotgun;
class UCameraComponent;
class UHeavyPartComponent;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
struct FInputActionValue;
struct FShotgunHitReport;

/** One damage pop-up for the HUD. */
struct FHeavyHitEvent
{
	FVector Location = FVector::ZeroVector;
	float Damage = 0.f;
	float Time = 0.f;
	bool bCritical = false;
	bool bKill = false;
};

/**
 * Third-person trooper tuned to feel heavy, Battlefront-style.
 *
 * The camera answers the mouse instantly; everything else has mass:
 *  - The body turns towards the camera on a slow spring (and faces the run direction while sprinting).
 *  - The shotgun aims on a faster, slightly under-damped spring, so it trails behind when you look
 *    around and swings past and settles when you stop. Aiming down sights makes it heavier still.
 *  - With bWeaponLagAffectsAim the reticle and the pellets follow the gun, not the screen centre,
 *    so the lag is something you play with rather than just see.
 *  - Movement has momentum (slow acceleration, low friction on direction changes), the torso leans
 *    into acceleration and rocks back when you stop, landings compress the body and the camera.
 *  - Recoil is impulses into those same springs: muzzle flip, kick back, body rock, camera punch.
 *
 * The trooper is built from primitives and posed procedurally (walk cycle, crouch, two-bone arm IK
 * onto the shotgun's grip and pump), so no skeletal mesh or animation assets are needed.
 */
UCLASS(config = Game)
class HEAVYSHOOTER_API AHeavyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHeavyCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnJumped_Implementation() override;

	// ---- Queries for the HUD ---------------------------------------------------------------

	AHeavyShotgun* GetWeapon() const { return Weapon; }

	/** Where the shotgun will fire (lags behind the camera when bWeaponLagAffectsAim). */
	FVector GetAimPoint() const { return AimPoint; }
	bool IsAimPointOnTarget() const { return bAimOnTarget; }

	/** Current pellet spread half-angle in degrees. */
	float GetCurrentSpread() const;

	/** How many degrees the gun is currently trailing the camera. */
	float GetWeaponLagDegrees() const;
	float GetAimAlpha() const { return AimAlpha.Value; }
	bool IsSprinting() const { return bIsSprinting; }
	bool ShouldShowHelp() const { return bShowHelp; }
	const TArray<FHeavyHitEvent>& GetHitEvents() const { return HitEvents; }
	float GetLastHitTime() const { return LastHitTime; }
	bool WasLastHitKill() const { return bLastHitKill; }
	float GetGameTime() const;

	// ---- Movement --------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float JogSpeed = 430.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float SprintSpeed = 660.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float AimWalkSpeed = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float CrouchSpeed = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float CrouchAimSpeed = 170.f;

	/** cm/s^2. Low values = more mass to get moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float JogAcceleration = 1400.f;

	/** Sprint builds up more slowly than jogging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float SprintAcceleration = 950.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float AimAcceleration = 1700.f;

	/** How much sideways input is allowed while sprinting (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float SprintStrafeScale = 0.45f;

	/** Fraction of speed lost right after the hardest landings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float LandingSpeedPenalty = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Movement")
	float LandingRecoveryTime = 0.4f;

	// ---- Camera ----------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float HipArmLength = 290.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float AimArmLength = 145.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float SprintArmLength = 320.f;

	/** Over-the-shoulder offset (Y is flipped when you swap shoulders). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	FVector HipCameraOffset = FVector(0.f, 58.f, 50.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	FVector AimCameraOffset = FVector(0.f, 66.f, 46.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float HipFOV = 85.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float AimFOV = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float SprintFOVBoost = 7.f;

	/** Camera position lag (higher = tighter). Rotation is never lagged: the weight is in the body and gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float CameraLagSpeedHip = 11.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float CameraLagSpeedAim = 18.f;

	/** Scales camera bob from footsteps. 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float CameraBobScale = 1.f;

	/** Degrees of camera rotation per unit of mouse input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float MouseSensitivity = 2.5f;

	/** Gamepad look speed in degrees per second at full stick (yaw, pitch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	FVector2D GamepadLookRate = FVector2D(180.f, 115.f);

	/** Look sensitivity multiplier while aiming down sights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float AimLookScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	bool bInvertLookY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float MinLookPitch = -70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Camera")
	float MaxLookPitch = 65.f;

	// ---- Weapon and body lag (the "weight") ------------------------------------------------

	/**
	 * When true the reticle and pellets follow where the lagging gun actually points. When false
	 * the lag is purely visual and shots always go to the screen centre.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag")
	bool bWeaponLagAffectsAim = true;

	/** Natural frequency (Hz) of the gun following the camera from the hip. Lower = more lag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag", meta = (ClampMin = "0.5"))
	float WeaponLagFrequencyHip = 5.0f;

	/** Below 1 the gun swings past and settles; 1 is no overshoot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag", meta = (ClampMin = "0.1"))
	float WeaponLagDampingHip = 0.65f;

	/** The gun never trails the camera by more than this many degrees from the hip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag")
	float WeaponMaxLagHip = 10.f;

	/** Aiming down sights: heavier (lower frequency) so the lag is most noticeable here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag", meta = (ClampMin = "0.5"))
	float WeaponLagFrequencyAim = 3.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag", meta = (ClampMin = "0.1"))
	float WeaponLagDampingAim = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag")
	float WeaponMaxLagAim = 7.f;

	/** How quickly the body turns to follow the camera (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag", meta = (ClampMin = "0.2"))
	float BodyTurnFrequency = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag", meta = (ClampMin = "0.2"))
	float BodyTurnFrequencyAim = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag", meta = (ClampMin = "0.1"))
	float BodyTurnDamping = 0.85f;

	/** The body never trails the camera by more than this many degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag")
	float BodyMaxLag = 65.f;

	/** Degrees of gun cant per degree/second of turning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag")
	float WeaponTurnRoll = 0.05f;

	/** Scales how far the gun shifts against changes in velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon Lag")
	float MovementSway = 1.f;

	// ---- Weapon poses (offsets from the chest, in aim space: X forward, Y right, Z up) -----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	FVector HipWeaponOffset = FVector(22.f, 19.f, -8.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	FVector AimWeaponOffset = FVector(18.f, 12.f, 6.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	FVector SprintWeaponOffset = FVector(10.f, 8.f, -18.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	FRotator SprintWeaponRotation = FRotator(-28.f, -40.f, -20.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	FVector ReloadWeaponOffset = FVector(-3.f, -5.f, -2.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	FRotator ReloadWeaponRotation = FRotator(6.f, -10.f, 30.f);

	/** How far the torso turns side-on to the target when holding / shouldering the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	float BladeAngleHip = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Poses")
	float BladeAngleAim = 22.f;

	// ---- Recoil (impulses into the springs above) -------------------------------------------

	/** Degrees the camera is pushed up per shot (you pull it back down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilViewKick = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilViewYawJitter = 0.5f;

	/** Degrees/second kicked into the gun's aim spring (moves the reticle, then settles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilAimKick = 70.f;

	/** Degrees/second of visual muzzle flip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilMuzzleFlip = 420.f;

	/** cm/s the gun is driven back into the shoulder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilKickBack = 280.f;

	/** Degrees/second of camera punch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilCameraPunch = 55.f;

	/** Camera shake trauma per shot (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilShake = 0.45f;

	/** Degrees/second the upper body rocks back per shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float RecoilBodyRock = 45.f;

	/** All recoil is multiplied by this when fully aimed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Recoil")
	float AimRecoilScale = 0.7f;

	// ---- Spread (half-angle, degrees) -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Spread")
	float HipSpread = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Spread")
	float AimSpread = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Spread")
	float MovingSpreadAdd = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Spread")
	float AirborneSpreadAdd = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Spread")
	float CrouchSpreadScale = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Spread")
	float SpreadBloomPerShot = 1.2f;

	/** Degrees of bloom recovered per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Spread")
	float SpreadBloomRecovery = 4.f;

	// ---- Weapon ------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heavy|Weapon")
	TSubclassOf<AHeavyShotgun> WeaponClass;

protected:
	virtual void BeginPlay() override;

private:
	// Construction
	void BuildBody();
	USceneComponent* MakeJoint(FName Name, USceneComponent* Parent, const FVector& Location);

	// Input
	void BuildInput();
	void Input_Move(const FInputActionValue& Value);
	void Input_MoveReleased();
	void Input_Look(const FInputActionValue& Value);
	void Input_LookStick(const FInputActionValue& Value);
	void Input_FirePressed();
	void Input_FireReleased();
	void Input_AimPressed();
	void Input_AimReleased();
	void Input_SprintPressed();
	void Input_SprintReleased();
	void Input_SprintToggle();
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_Crouch();
	void Input_Reload();
	void Input_SwapShoulder();
	void Input_ToggleHelp();

	/** Rotates the view directly (bypasses the engine's legacy input scales). */
	void AddViewRotation(float YawDelta, float PitchDelta);
	FRotator GetLookRotation() const;

	// Per frame, in order
	void UpdateMovementState(float DeltaTime);
	void UpdateBodyAndAim(float DeltaTime);
	void UpdateAimPoint();
	void UpdateTorsoAndLegs(float DeltaTime);
	void UpdateWeaponPose(float DeltaTime);
	void UpdateFiring();
	void UpdateArms();
	void UpdateCamera(float DeltaTime);

	FVector TraceAim(const FRotator& Rotation, bool& bOutOnTarget) const;
	void ApplyRecoil();
	void RecordHits(const TArray<FShotgunHitReport>& Hits);
	void AddTrauma(float Amount);

	// ---- Components ------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> BodyRoot;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> Pelvis;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> Spine;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> Neck;

	/** Chest pivot that the gun is held around; turned to face the (lagging) aim point. */
	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> AimPivot;

	/** The shotgun is attached here; posed every frame. */
	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> WeaponMount;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> ShoulderL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> ShoulderR;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> ReloadPouch;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> ThighL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> ThighR;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> KneeL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> KneeR;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> AnkleL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<USceneComponent> AnkleR;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> UpperArmL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> UpperArmR;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> ForearmL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> ForearmR;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> ElbowL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> ElbowR;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> HandL;

	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> HandR;

	/** The shell in the support hand while reloading. */
	UPROPERTY(VisibleAnywhere, Category = "Heavy|Components")
	TObjectPtr<UHeavyPartComponent> HandShell;

	UPROPERTY(Transient)
	TObjectPtr<AHeavyShotgun> Weapon;

	// ---- Input objects (created in code, no assets) -----------------------------------------

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> InputContext;

	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AimAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintToggleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ShoulderAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HelpAction;

	// ---- Input state -----------------------------------------------------------------------

	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bFireHeld = false;
	bool bAimHeld = false;
	bool bSprintHeld = false;
	bool bSprintToggled = false;
	bool bIsSprinting = false;
	bool bShowHelp = true;
	float ShoulderSign = 1.f;
	float FireBufferedUntil = -1.f;
	float LastFireTime = -100.f;

	// ---- Simulation state ------------------------------------------------------------------

	FHeavySpring BodyYaw;
	FHeavySpring AimYaw;
	FHeavySpring AimPitch;

	FHeavySpring AimAlpha;
	FHeavySpring SprintAlpha;
	FHeavySpring CrouchAlpha;
	FHeavySpring ReloadAlpha;
	FHeavySpring AirAlpha;
	FHeavySpring ShoulderSide;

	FHeavySpring LeanForward;
	FHeavySpring LeanRight;
	FHeavySpring HipDrop;

	FHeavySpringVector WeaponSway;
	FHeavySpringVector WeaponKick;
	FHeavySpring MuzzleFlip;
	FHeavySpring KickRoll;

	FHeavySpring CameraDip;
	FHeavySpring CameraPunch;
	FHeavySpring CameraRoll;

	FVector LastVelocity = FVector::ZeroVector;
	FVector SmoothedAcceleration = FVector::ZeroVector;
	float SpeedAlpha = 0.f;
	float StridePhase = 0.f;
	float LastAirVelocityZ = 0.f;
	float LandingPenalty = 0.f;
	float Trauma = 0.f;
	float SpreadBloom = 0.f;

	FVector AimPoint = FVector::ZeroVector;
	FVector VisualAimPoint = FVector::ZeroVector;
	bool bAimOnTarget = false;

	TArray<FHeavyHitEvent> HitEvents;
	float LastHitTime = -100.f;
	bool bLastHitKill = false;
};
