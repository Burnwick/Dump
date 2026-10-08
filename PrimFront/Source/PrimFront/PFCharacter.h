#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PFCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputAction;
class UInputMappingContext;
class APFShotgun;
struct FInputActionValue;
struct FKey;

UENUM()
enum class EPFReloadPhase : uint8
{
	None,
	Start,  // hand moves from the pump to the loading port
	Insert, // one shell per cycle
	Finish  // rack the pump (only when reloading from empty)
};

/** Two joints of a primitive limb: shoulder/elbow or hip/knee. Parts hang along the joint's -Z axis. */
USTRUCT()
struct FPFLimb
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> Mid = nullptr;

	float UpperLength = 0.f;
	float LowerLength = 0.f;
};

/**
 * Third person shotgun trooper. The body is made of primitive shapes animated procedurally
 * (walk cycle, two-bone IK arms on the shotgun, crouch, holster, melee).
 * Input actions and key bindings are created in code, so no input assets are needed.
 */
UCLASS()
class PRIMFRONT_API APFCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APFCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PostInitializeComponents() override;

	// ---- Queries for the HUD ----
	APFShotgun* GetWeapon() const { return Weapon; }
	float GetAimAlpha() const { return AimAlpha; }
	bool IsSprinting() const { return bSprinting; }
	bool IsWeaponHolstered() const { return bHolstered; }
	bool IsWeaponOut() const { return !bHolstered && HolsterAlpha <= 0.05f; }
	bool IsReloading() const { return ReloadPhase != EPFReloadPhase::None; }
	bool IsMeleeing() const { return MeleeTimer > 0.f; }
	bool IsShoulderRight() const { return ShoulderSide > 0.f; }
	bool ShouldShowHelp() const { return bShowHelp; }
	float GetSpreadDegrees() const;
	float GetCameraFOV() const;

	// ---- Range stats ----
	int32 ShotsFired = 0;
	int32 PelletsFired = 0;
	int32 PelletsHit = 0;
	int32 Kills = 0;
	int32 Headshots = 0;
	int32 MeleeHits = 0;
	float HitMarkerTime = 0.f;
	bool bHitMarkerKill = false;
	bool bHitMarkerHead = false;

	// ---- Tuning ----
	UPROPERTY(EditAnywhere, Category = "Movement")
	float RunSpeed = 430.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float AimWalkSpeed = 250.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintSpeed = 680.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float CrouchSpeed = 230.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float HipArmLength = 260.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float AdsArmLength = 120.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float SprintArmLength = 290.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float HipShoulderOffset = 55.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float AdsShoulderOffset = 45.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float HipFOV = 90.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float AdsFOV = 60.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float SprintFOV = 96.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float StandingCameraHeight = 160.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float CrouchedCameraHeight = 112.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float LookSensitivity = 1.f;

	/** Look sensitivity multiplier while aiming down sights. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float AdsSensitivityScale = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float HipSpread = 6.5f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float AdsSpread = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float RecoilPitchHip = 2.4f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float RecoilPitchAds = 1.4f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float RecoilYaw = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ReloadStartTime = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ShellInsertTime = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ReloadFinishTime = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float HolsterTime = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ArmedMeleeTime = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float UnarmedMeleeTime = 0.42f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ArmedMeleeDamage = 55.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float UnarmedMeleeDamage = 35.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleeRange = 120.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleeRadius = 60.f;

	/** Velocity change (cm/s) given to physics props hit by melee. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float MeleePush = 650.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// ---- Input (PFCharacter.cpp) ----
	UInputAction* MakeAction(const TCHAR* Name, bool bAxis2D);
	void MapKey(UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegate = false, bool bNegateYOnly = false);
	void CreateInput();

	void OnMove(const FInputActionValue& Value);
	void OnMoveStop();
	void OnLook(const FInputActionValue& Value);
	void OnJumpPressed();
	void OnJumpReleased();
	void OnFirePressed();
	void OnAimPressed();
	void OnAimReleased();
	void OnSprintPressed();
	void OnSprintReleased();
	void OnCrouchPressed();
	void OnReloadPressed();
	void OnShoulderSwapPressed();
	void OnHolsterPressed();
	void OnMeleePressed();
	void OnResetRangePressed();
	void OnHelpPressed();

	// ---- State, movement and camera (PFCharacter.cpp) ----
	void UpdateMovementState(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void UpdateAimPoint();
	bool CanAim() const;

	// ---- Combat (PFCharacterCombat.cpp) ----
	void UpdateCombat(float DeltaSeconds);
	bool CanFireNow() const;
	bool TryFire();
	void FireShot();
	void StartReload();
	void CancelReload();
	void TickReload(float DeltaSeconds);
	void StartMelee();
	void DoMeleeHit();
	void SetHolstered(bool bNewHolstered);
	void RegisterHit(bool bKill, bool bHead);
	float GetMeleeProgress() const;
	float GetPumpAlpha() const;

	// ---- Body (PFCharacterBody.cpp) ----
	void BuildBody();
	void BuildArm(FPFLimb& Arm, float Side);
	void BuildLeg(FPFLimb& Leg, float Side);
	void UpdateBody(float DeltaSeconds);
	FTransform ComputeWeaponPose() const;
	void UpdateArms(const FTransform& BodyXf, float MoveAlpha);
	FVector SolveLimb(FPFLimb& Limb, const FVector& Target, const FVector& PoleDir);

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient)
	TObjectPtr<APFShotgun> Weapon;

	// Input objects (created at runtime)
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> AimAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ShoulderAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HolsterAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MeleeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ResetAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HelpAction;

	// Body joints and parts (created at runtime)
	UPROPERTY(Transient) TObjectPtr<USceneComponent> BodyRoot;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Pelvis;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Spine;
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Neck;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FootL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FootR;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldShell;
	UPROPERTY(Transient) FPFLimb ArmL;
	UPROPERTY(Transient) FPFLimb ArmR;
	UPROPERTY(Transient) FPFLimb LegL;
	UPROPERTY(Transient) FPFLimb LegR;

	// Input state
	FVector2D MoveInput = FVector2D::ZeroVector;
	bool bAimHeld = false;
	bool bSprintHeld = false;
	bool bShowHelp = true;

	// Movement / camera state
	bool bSprinting = false;
	float AimAlpha = 0.f;
	float SprintAlpha = 0.f;
	float CrouchAlpha = 0.f;
	float AirAlpha = 0.f;
	float ShoulderSide = 1.f;
	float ShoulderAlpha = 1.f;
	float CameraHeight = 160.f;
	FVector AimPoint = FVector::ZeroVector;

	// Combat state
	bool bHolstered = false;
	float HolsterAlpha = 0.f;
	float FireCooldown = 0.f;
	float FireQueueTime = 0.f;
	float TimeSinceShot = 10.f;
	float RecoilKick = 0.f;
	float SpreadBloom = 0.f;
	EPFReloadPhase ReloadPhase = EPFReloadPhase::None;
	float ReloadTimer = 0.f;
	float ReloadAlpha = 0.f;
	bool bReloadInterrupt = false;
	bool bReloadNeedsPump = false;
	float MeleeTimer = 0.f;
	float MeleeDuration = 0.5f;
	bool bMeleeArmed = false;
	bool bMeleeHitDone = false;
	bool bPunchLeft = false;

	// Animation state
	float WalkPhase = 0.f;
};
