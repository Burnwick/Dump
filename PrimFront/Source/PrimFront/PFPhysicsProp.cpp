#include "PFPhysicsProp.h"
#include "PFShapes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APFPhysicsProp::APFPhysicsProp()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Mesh->SetSimulatePhysics(true);
	SetRootComponent(Mesh);
}

void APFPhysicsProp::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Mesh->SetMassOverrideInKg(NAME_None, MassKg, true);
	SetColor(Color);
}

void APFPhysicsProp::SetColor(const FLinearColor& NewColor)
{
	Color = NewColor;
	FPFShapes::SetColor(Mesh, Color);
}
