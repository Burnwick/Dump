#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PFRangeBuilder.generated.h"

class APFTarget;
class UStaticMeshComponent;

/**
 * Builds the whole shooting range out of primitives at runtime: floor, lanes, benches, berm,
 * distance signs, pop-up targets (static and moving), a melee yard with physics crates and a
 * small cover course. Also adds a sun, sky light and atmosphere if the level has none.
 * The game mode spawns one automatically; you can also drop one into your own level.
 */
UCLASS()
class PRIMFRONT_API APFRangeBuilder : public AActor
{
	GENERATED_BODY()

public:
	APFRangeBuilder();

	virtual void BeginPlay() override;

	/** Builds everything once (safe to call repeatedly). */
	void Build();

	/** Where the player spawns when the level has no PlayerStart (behind the firing line, facing downrange). */
	static FTransform GetPlayerSpawnTransform();

	UPROPERTY(EditAnywhere, Category = "Range")
	bool bSpawnLighting = true;

	UPROPERTY(EditAnywhere, Category = "Range")
	bool bBuildFloor = true;

private:
	void BuildGeometry();
	void BuildSigns();
	void SpawnLighting();
	void SpawnTargets();
	void SpawnProps();

	UStaticMeshComponent* Block(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollision = true);
	void Label(const FString& Text, const FVector& Location, float WorldSize, const FColor& Color);
	APFTarget* SpawnTarget(const FVector& LocalPosition);
	void SpawnCrate(const FVector& LocalPosition, float Size, const FLinearColor& Color);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	bool bBuilt = false;
};
