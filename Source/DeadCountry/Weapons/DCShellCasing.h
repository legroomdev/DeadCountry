#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCShellCasing.generated.h"

class UStaticMeshComponent;

/**
 * A spent casing flicked out of the ejection port. Purely cosmetic: spawned locally on every
 * machine, never replicated, capped in number, tinks when it lands.
 */
UCLASS()
class DEADCOUNTRY_API ADCShellCasing : public AActor
{
	GENERATED_BODY()

public:
	ADCShellCasing();

	/** Spawns a casing at the port and throws it right/up/back, adding the shooter's velocity. */
	static void Eject(UWorld* World, const FTransform& PortTransform, const FVector& InheritedVelocity);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Casing")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UFUNCTION()
	void OnCasingHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	float LastTinkTime = -1.f;
	int32 TinkCount = 0;

	static constexpr int32 MaxLiveCasings = 40;
	static TArray<TWeakObjectPtr<ADCShellCasing>> LiveCasings;
};
