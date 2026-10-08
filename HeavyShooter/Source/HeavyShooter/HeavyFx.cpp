#include "HeavyFx.h"

#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"

AHeavyTransientFx::AHeavyTransientFx()
{
	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(false);

	Mesh = CreateDefaultSubobject<UHeavyPartComponent>(TEXT("Mesh"));
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;
}

AHeavyTransientFx* AHeavyTransientFx::Spawn(UWorld* World, EHeavyShape Shape, const FTransform& Transform, const FLinearColor& Color,
	float InLifetime, const FVector& InEndScale, const FVector& InVelocity, float InGravity)
{
	if (!World)
	{
		return nullptr;
	}

	AHeavyTransientFx* Fx = World->SpawnActorDeferred<AHeavyTransientFx>(AHeavyTransientFx::StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Fx)
	{
		return nullptr;
	}

	// Native components are already registered by now, so paint explicitly after the mesh swap.
	Fx->Mesh->SetStaticMesh(UHeavyPartComponent::GetShapeMesh(Shape));
	Fx->Mesh->SetPartColor(Color);
	Fx->StartScale = Transform.GetScale3D();
	Fx->EndScale = InEndScale;
	Fx->Velocity = InVelocity;
	Fx->Gravity = InGravity;
	Fx->Lifetime = FMath::Max(InLifetime, 0.01f);
	Fx->FinishSpawning(Transform);
	return Fx;
}

void AHeavyTransientFx::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}

	if (!Velocity.IsNearlyZero() || Gravity != 0.f)
	{
		Velocity.Z -= Gravity * DeltaSeconds;
		AddActorWorldOffset(Velocity * DeltaSeconds);
	}

	const float Alpha = Age / Lifetime;
	SetActorScale3D(FMath::Lerp(StartScale, EndScale, Alpha));
}

AHeavyShellCasing::AHeavyShellCasing()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 7.f;
	SetCanBeDamaged(false);

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(3.2f, 1.05f, 1.05f));
	Collision->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// Shells bounce off the world but never block the player, the camera or bullets.
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetSimulatePhysics(true);
	Collision->SetMassOverrideInKg(NAME_None, 0.05f, true);
	Collision->BodyInstance.bUseCCD = true;
	Collision->SetLinearDamping(0.2f);
	Collision->SetAngularDamping(0.6f);
	Collision->SetCanEverAffectNavigation(false);
	RootComponent = Collision;

	Hull = UHeavyPartComponent::CreateRodX(this, TEXT("Hull"), Collision, FVector(0.6f, 0.f, 0.f), 2.0f, 5.2f, FLinearColor(0.55f, 0.05f, 0.04f));
	Base = UHeavyPartComponent::CreateRodX(this, TEXT("Base"), Collision, FVector(-2.6f, 0.f, 0.f), 2.1f, 1.2f, FLinearColor(0.78f, 0.56f, 0.22f));
	Hull->SetCastShadow(false);
	Base->SetCastShadow(false);
}

void AHeavyShellCasing::Launch(const FVector& LinearVelocity, const FVector& AngularVelocityDegrees)
{
	Collision->SetPhysicsLinearVelocity(LinearVelocity);
	Collision->SetPhysicsAngularVelocityInDegrees(AngularVelocityDegrees);
}
