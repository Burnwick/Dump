#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavySpring.h"
#include "HeavyTargetDummy.generated.h"

class UHeavyPartComponent;

/**
 * A pop-up range target. Rocks back on a spring when hit (the heavier the hit, the bigger
 * the rock), takes extra damage to the head, slams flat when killed and stands back up
 * after RespawnDelay. Can optionally strafe side to side.
 *
 * Faces its local +X axis.
 */
UCLASS()
class HEAVYSHOOTER_API AHeavyTargetDummy : public AActor
{
	GENERATED_BODY()

public:
	AHeavyTargetDummy();

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	bool IsDown() const { return bDown; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	FVector GetHealthBarLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	float MaxHealth = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	float HeadshotMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	float RespawnDelay = 3.f;

	/** Degrees per second of wobble added per point of damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	float HitRock = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Movement")
	bool bStrafe = false;

	/** Half-width of the strafe path (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Movement")
	float StrafeDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy|Movement")
	float StrafeSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dummy")
	FLinearColor BodyColor = FLinearColor(0.55f, 0.47f, 0.33f);

protected:
	virtual void BeginPlay() override;

private:
	void RefreshColors();

	UPROPERTY(VisibleAnywhere, Category = "Dummy")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Dummy")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, Category = "Dummy")
	TObjectPtr<UHeavyPartComponent> Base;

	UPROPERTY(VisibleAnywhere, Category = "Dummy")
	TObjectPtr<UHeavyPartComponent> Post;

	UPROPERTY(VisibleAnywhere, Category = "Dummy")
	TObjectPtr<UHeavyPartComponent> Torso;

	UPROPERTY(VisibleAnywhere, Category = "Dummy")
	TObjectPtr<UHeavyPartComponent> Head;

	FHeavySpring TiltForward;
	FHeavySpring TiltRight;
	FVector2D FallDirection = FVector2D(1.f, 0.f);

	FVector HomeLocation = FVector::ZeroVector;
	float StrafePhase = 0.f;

	float Health = 0.f;
	bool bDown = false;
	float DownTime = 0.f;
	float HitFlash = 0.f;
	bool bHeadFlash = false;
};
