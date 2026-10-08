#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeavyTestRange.generated.h"

/**
 * Builds a lit firing range at BeginPlay: a checkered floor (so you can feel your speed),
 * perimeter walls, cover of different heights, a ramp and a ledge to drop off for heavy
 * landings, a shooting lane of dummies at 6/12/20/30 m, strafing dummies and stacks of
 * physics crates and barrels for the shotgun to push around.
 *
 * AHeavyGameMode spawns one automatically when the level is empty. You can also drop
 * it into any level. Everything is built relative to the actor's transform; the
 * player is meant to start at PlayerStartOffset facing +X.
 */
UCLASS()
class HEAVYSHOOTER_API AHeavyTestRange : public AActor
{
	GENERATED_BODY()

public:
	AHeavyTestRange();

	/** Where the player should start, relative to the range. */
	static FVector GetPlayerStartOffset() { return FVector(-2600.f, 0.f, 110.f); }

	/** Add a sun, sky, sky light and fog if the level has no directional light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range")
	bool bAddLightingIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range")
	bool bBuildFloor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range")
	bool bSpawnTargets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Range")
	bool bSpawnPhysicsProps = true;

protected:
	virtual void BeginPlay() override;

private:
	void BuildLighting();
	void BuildGeometry();
	void SpawnTargets();
	void SpawnProps();
	void Block(const FVector& Center, const FVector& Size, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator);

	UPROPERTY(VisibleAnywhere, Category = "Range")
	TObjectPtr<USceneComponent> Root;
};

/** A box or barrel with real physics; the shotgun shoves these around. */
UCLASS()
class HEAVYSHOOTER_API AHeavyPhysicsProp : public AActor
{
	GENERATED_BODY()

public:
	AHeavyPhysicsProp();

	void Setup(bool bBarrel, const FVector& SizeCm, const FLinearColor& Color, float MassKg);

private:
	UPROPERTY(VisibleAnywhere, Category = "Prop")
	TObjectPtr<class UHeavyPartComponent> Body;
};
