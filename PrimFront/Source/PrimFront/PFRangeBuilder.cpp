#include "PFRangeBuilder.h"
#include "PFPhysicsProp.h"
#include "PFShapes.h"
#include "PFTarget.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// Range layout (cm, builder-local): +X is downrange, the firing line is at X = 0, lanes span Y = -1000..1000.
namespace
{
	const FLinearColor RangeFloorColor(0.28f, 0.29f, 0.3f);
	const FLinearColor RangePadColor(0.42f, 0.42f, 0.4f);
	const FLinearColor RangeLineColor(0.95f, 0.75f, 0.05f);
	const FLinearColor RangeStripeColor(0.6f, 0.6f, 0.58f);
	const FLinearColor RangeConcreteColor(0.55f, 0.54f, 0.5f);
	const FLinearColor RangeBenchColor(0.3f, 0.22f, 0.14f);
	const FLinearColor RangeBermColor(0.32f, 0.24f, 0.16f);
	const FLinearColor RangeWallColor(0.48f, 0.5f, 0.52f);
	const FLinearColor RangeSignColor(0.1f, 0.12f, 0.15f);
	const FLinearColor RangeYardColor(0.24f, 0.32f, 0.24f);
	const FLinearColor RangeCrateColorA(0.45f, 0.3f, 0.12f);
	const FLinearColor RangeCrateColorB(0.3f, 0.36f, 0.2f);

	constexpr float LaneWidth = 400.f;
	constexpr int32 NumLanes = 5;
}

APFRangeBuilder::APFRangeBuilder()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void APFRangeBuilder::BeginPlay()
{
	Super::BeginPlay();
	Build(); // for builders placed by hand in a level
}

FTransform APFRangeBuilder::GetPlayerSpawnTransform()
{
	return FTransform(FRotator::ZeroRotator, FVector(-300.f, 0.f, 110.f));
}

void APFRangeBuilder::Build()
{
	if (bBuilt || !GetWorld())
	{
		return;
	}
	bBuilt = true;

	if (bSpawnLighting)
	{
		SpawnLighting();
	}
	BuildGeometry();
	BuildSigns();
	SpawnTargets();
	SpawnProps();
}

UStaticMeshComponent* APFRangeBuilder::Block(const FVector& Center, const FVector& Size, const FLinearColor& Color, bool bCollision)
{
	return FPFShapes::AddPart(this, Root, EPFShape::Cube, Size, Center, FRotator::ZeroRotator, Color, bCollision);
}

void APFRangeBuilder::Label(const FString& Text, const FVector& Location, float WorldSize, const FColor& Color)
{
	UTextRenderComponent* TextComp = NewObject<UTextRenderComponent>(this);
	TextComp->SetMobility(EComponentMobility::Movable);
	TextComp->SetupAttachment(Root);
	TextComp->SetRelativeLocationAndRotation(Location, FRotator(0.f, 180.f, 0.f)); // face back up the range
	TextComp->SetHorizontalAlignment(EHTA_Center);
	TextComp->SetVerticalAlignment(EVRTA_TextCenter);
	TextComp->SetWorldSize(WorldSize);
	TextComp->SetTextRenderColor(Color);
	TextComp->SetText(FText::FromString(Text));
	TextComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TextComp->SetCastShadow(false);
	TextComp->RegisterComponent();
	AddInstanceComponent(TextComp);
}

void APFRangeBuilder::BuildGeometry()
{
	const float HalfLanes = LaneWidth * NumLanes * 0.5f;

	if (bBuildFloor)
	{
		Block(FVector(1800.f, 0.f, -10.f), FVector(8400.f, 6000.f, 20.f), RangeFloorColor);
	}

	// Firing point: pad, yellow line and benches with gaps to walk downrange
	Block(FVector(-500.f, 0.f, 1.f), FVector(1000.f, HalfLanes * 2.f + 400.f, 2.f), RangePadColor);
	Block(FVector(5.f, 0.f, 1.f), FVector(10.f, HalfLanes * 2.f + 400.f, 2.f), RangeLineColor, false);
	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		const float LaneY = -HalfLanes + LaneWidth * (Lane + 0.5f);
		Block(FVector(60.f, LaneY, 47.5f), FVector(50.f, 160.f, 95.f), RangeBenchColor);
	}

	// Lane boundary stripes on the floor and low dividers for the first 10m
	for (int32 i = 0; i <= NumLanes; ++i)
	{
		const float Y = -HalfLanes + LaneWidth * i;
		Block(FVector(2500.f, Y, 0.5f), FVector(5000.f, 6.f, 1.f), RangeStripeColor, false);
		if (i > 0 && i < NumLanes)
		{
			Block(FVector(600.f, Y, 60.f), FVector(1000.f, 16.f, 120.f), RangeConcreteColor);
		}
	}

	// Distance lines across the range
	for (const float X : { 1000.f, 2000.f, 2700.f, 3500.f, 4500.f })
	{
		Block(FVector(X, 0.f, 0.5f), FVector(8.f, HalfLanes * 2.f, 1.f), RangeStripeColor, false);
	}

	// Berm, side walls and back wall
	Block(FVector(5300.f, 0.f, 400.f), FVector(600.f, 6000.f, 800.f), RangeBermColor);
	Block(FVector(4950.f, 0.f, 100.f), FVector(100.f, 6000.f, 200.f), RangeBermColor);
	Block(FVector(1800.f, -3000.f, 250.f), FVector(8400.f, 40.f, 500.f), RangeWallColor);
	Block(FVector(1800.f, 3000.f, 250.f), FVector(8400.f, 40.f, 500.f), RangeWallColor);
	Block(FVector(-2400.f, 0.f, 250.f), FVector(40.f, 6000.f, 500.f), RangeWallColor);

	// Melee / CQB yard (right of the lanes): pad, low walls for crouching, one tall wall
	Block(FVector(-400.f, 2100.f, 1.f), FVector(2400.f, 1400.f, 2.f), RangeYardColor);
	Block(FVector(-900.f, 1750.f, 55.f), FVector(40.f, 300.f, 110.f), RangeConcreteColor);
	Block(FVector(-900.f, 2450.f, 55.f), FVector(40.f, 300.f, 110.f), RangeConcreteColor);
	Block(FVector(-1350.f, 2100.f, 150.f), FVector(40.f, 400.f, 300.f), RangeConcreteColor);

	// Cover course (left of the lanes): mixed-height blocks
	Block(FVector(-700.f, -1700.f, 55.f), FVector(220.f, 40.f, 110.f), RangeConcreteColor);
	Block(FVector(-300.f, -2150.f, 120.f), FVector(40.f, 220.f, 240.f), RangeConcreteColor);
	Block(FVector(200.f, -1800.f, 40.f), FVector(160.f, 160.f, 80.f), RangeConcreteColor);
	Block(FVector(600.f, -2400.f, 55.f), FVector(220.f, 40.f, 110.f), RangeConcreteColor);
	Block(FVector(900.f, -1900.f, 150.f), FVector(60.f, 60.f, 300.f), RangeConcreteColor);
}

void APFRangeBuilder::BuildSigns()
{
	const float SignY = LaneWidth * NumLanes * 0.5f + 150.f;
	struct FDistanceSign
	{
		float X;
		const TCHAR* Text;
	};
	const FDistanceSign Distances[] = {
		{ 1000.f, TEXT("10 m") }, { 2000.f, TEXT("20 m") }, { 2700.f, TEXT("27 m") }, { 3500.f, TEXT("35 m") }, { 4500.f, TEXT("45 m") } };

	for (const FDistanceSign& Dist : Distances)
	{
		for (const float Side : { -1.f, 1.f })
		{
			const float Y = SignY * Side;
			Block(FVector(Dist.X, Y, 75.f), FVector(14.f, 14.f, 150.f), RangeSignColor);
			Block(FVector(Dist.X, Y, 175.f), FVector(6.f, 110.f, 55.f), RangeSignColor);
			Label(Dist.Text, FVector(Dist.X - 4.f, Y, 175.f), 36.f, FColor(255, 200, 40));
		}
	}

	Label(TEXT("PRIMFRONT FIRING RANGE"), FVector(4890.f, 0.f, 520.f), 150.f, FColor(230, 230, 230));
	Label(TEXT("T  =  RESET TARGETS + AMMO"), FVector(4890.f, 0.f, 380.f), 70.f, FColor(255, 200, 40));
	Label(TEXT("MELEE YARD"), FVector(-1330.f, 2100.f, 260.f), 50.f, FColor(255, 255, 255));
}

void APFRangeBuilder::SpawnLighting()
{
	UWorld* World = GetWorld();

	bool bHasSun = false;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		bHasSun = true;
	}
	bool bHasSkyLight = false;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		bHasSkyLight = true;
	}
	UClass* AtmosphereClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.SkyAtmosphere"));
	bool bHasAtmosphere = false;
	if (AtmosphereClass)
	{
		for (TActorIterator<AActor> It(World, AtmosphereClass); It; ++It)
		{
			bHasAtmosphere = true;
		}
	}

	if (!bHasSun)
	{
		const FTransform SunXf(FRotator(-42.f, 35.f, 0.f));
		if (ADirectionalLight* Sun = World->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunXf, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				SunLight->SetMobility(EComponentMobility::Movable);
				SunLight->SetIntensity(10.f);
				SunLight->SetAtmosphereSunLight(true);
			}
			Sun->FinishSpawning(SunXf);
		}
	}

	if (!bHasAtmosphere && AtmosphereClass)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AActor>(AtmosphereClass, FTransform::Identity, Params);
	}

	if (!bHasSkyLight)
	{
		if (ASkyLight* Sky = World->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			if (USkyLightComponent* SkyComp = Sky->GetLightComponent())
			{
				SkyComp->SetMobility(EComponentMobility::Movable);
				SkyComp->bRealTimeCapture = true; // lights the range from the atmosphere, no cubemap needed
			}
			Sky->FinishSpawning(FTransform::Identity);
		}
	}
}

APFTarget* APFRangeBuilder::SpawnTarget(const FVector& LocalPosition)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Targets face back up the range (their +X towards the firing line)
	const FTransform Xf(GetActorRotation() + FRotator(0.f, 180.f, 0.f), GetActorTransform().TransformPosition(LocalPosition));
	return GetWorld()->SpawnActor<APFTarget>(APFTarget::StaticClass(), Xf, Params);
}

void APFRangeBuilder::SpawnTargets()
{
	const float HalfLanes = LaneWidth * NumLanes * 0.5f;

	// 10m: one per lane
	for (int32 Lane = 0; Lane < NumLanes; ++Lane)
	{
		SpawnTarget(FVector(1000.f, -HalfLanes + LaneWidth * (Lane + 0.5f), 0.f));
	}

	// 20m and 35m
	for (const float Y : { -600.f, 0.f, 600.f })
	{
		SpawnTarget(FVector(2000.f, Y, 0.f));
	}
	for (const float Y : { -400.f, 400.f })
	{
		SpawnTarget(FVector(3500.f, Y, 0.f));
	}

	// Moving targets sweeping across the lanes
	if (APFTarget* Mover = SpawnTarget(FVector(2700.f, 0.f, 0.f)))
	{
		Mover->SetMovement(700.f, 0.15f);
	}
	if (APFTarget* Mover = SpawnTarget(FVector(4500.f, 0.f, 0.f)))
	{
		Mover->SetMovement(850.f, 0.1f);
	}

	// Melee yard dummies
	for (const float Y : { 1750.f, 2100.f, 2450.f })
	{
		SpawnTarget(FVector(400.f, Y, 0.f));
	}

	// Cover course dummies
	SpawnTarget(FVector(1400.f, -1700.f, 0.f));
	SpawnTarget(FVector(1800.f, -2400.f, 0.f));
}

void APFRangeBuilder::SpawnCrate(const FVector& LocalPosition, float Size, const FLinearColor& Color)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform Xf(GetActorRotation(), GetActorTransform().TransformPosition(LocalPosition), FVector(Size / 100.f));
	if (APFPhysicsProp* Crate = GetWorld()->SpawnActor<APFPhysicsProp>(APFPhysicsProp::StaticClass(), Xf, Params))
	{
		Crate->SetColor(Color);
	}
}

void APFRangeBuilder::SpawnProps()
{
	// Pyramid of crates in the melee yard
	const float Crate = 60.f;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const int32 Count = 3 - Row;
		for (int32 i = 0; i < Count; ++i)
		{
			const float Y = 2100.f + (i - (Count - 1) * 0.5f) * (Crate + 4.f);
			const float Z = Crate * 0.5f + 2.f + Row * (Crate + 1.f);
			SpawnCrate(FVector(-150.f, Y, Z), Crate, (Row + i) % 2 == 0 ? RangeCrateColorA : RangeCrateColorB);
		}
	}

	// A few loose boxes to knock around
	SpawnCrate(FVector(100.f, 1600.f, 42.f), 80.f, RangeCrateColorB);
	SpawnCrate(FVector(150.f, 2650.f, 27.f), 50.f, RangeCrateColorA);
	SpawnCrate(FVector(-500.f, 2300.f, 22.f), 40.f, RangeCrateColorA);

	// Something to shoot at downrange too
	SpawnCrate(FVector(1500.f, -300.f, 32.f), 60.f, RangeCrateColorA);
	SpawnCrate(FVector(1500.f, 300.f, 32.f), 60.f, RangeCrateColorB);
}
