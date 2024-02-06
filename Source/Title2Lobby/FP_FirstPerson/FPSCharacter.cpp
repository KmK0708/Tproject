// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSCharacter.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
//플러그인 weaponbase추가
#include "Weapons/WeaponBase.h"

// Sets default values
AFPSCharacter::AFPSCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	// 3인칭 메시 1인칭 머리에 카메라 
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>("Camera");
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetupAttachment(GetMesh(), FName("head"));
}

// Called when the game starts or when spawned
void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AFPSCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D InputValue = Value.Get<FVector2D>();

	if(InputValue.X != 0.0f)
	{
		AddMovementInput(GetActorRightVector(), InputValue.X);
	}

	if(InputValue.Y != 0.0f)
	{
		AddMovementInput(GetActorForwardVector(), InputValue.Y);	
	}
}

void AFPSCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D InputValue = Value.Get<FVector2D>();

	if(InputValue.X != 0.0f)
	{
		AddControllerYawInput(InputValue.X);
	}

	if(InputValue.Y != 0.0f)
	{
		AddControllerPitchInput(InputValue.Y);	
	}
}

void AFPSCharacter::StartFire(const FInputActionValue& Value)
{
	AWeaponBase* pWeapon = Cast<AWeaponBase>(m_EquipWeapon);
	// pweapon의 m_ammo가 0이면 return
	if (IsValid(pWeapon) && pWeapon->m_Ammo <= 0)
		return;
	ReqTrigger();
	UE_LOG(LogTemp, Warning, TEXT("Fire"));
}

void AFPSCharacter::StopFire(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Warning, TEXT("StopFire"));
	AWeaponBase* pWeapon = Cast<AWeaponBase>(m_EquipWeapon);
	if (IsValid(pWeapon))
	{
		pWeapon->GetWorldTimerManager().ClearTimer(pWeapon->TimerHandle_ShotDelay);
	}
}

void AFPSCharacter::PickUp(const FInputActionValue& Value)
{
	ReqPickUp();
}

void AFPSCharacter::Reload(const FInputActionValue& Value)
{
	ReqReload();
}

void AFPSCharacter::Drop(const FInputActionValue& Value)
{
	ReqDrop();
}

// Called every frame
void AFPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EnhancedPlayerInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (EnhancedPlayerInputComponent == nullptr || PlayerController == nullptr)
	{
		return;
	}
	
	UEnhancedInputLocalPlayerSubsystem* EnhancedSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());

	if(EnhancedSubsystem == nullptr)
	{
		return;
	}

	EnhancedSubsystem->AddMappingContext(InputMappingContext, 1);

	EnhancedPlayerInputComponent->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AFPSCharacter::Move);
	EnhancedPlayerInputComponent->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AFPSCharacter::Look);
	EnhancedPlayerInputComponent->BindAction(JumpInputAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	EnhancedPlayerInputComponent->BindAction(FireInputAction, ETriggerEvent::Started, this, &AFPSCharacter::StartFire);
	EnhancedPlayerInputComponent->BindAction(FireInputAction, ETriggerEvent::Completed, this, &AFPSCharacter::StopFire);
	EnhancedPlayerInputComponent->BindAction(PickUpAction, ETriggerEvent::Triggered, this, &AFPSCharacter::PickUp);
	EnhancedPlayerInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &AFPSCharacter::Reload);
	EnhancedPlayerInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &AFPSCharacter::Drop);
}

void AFPSCharacter::Fire()
{
	//if (HasAuthority()) {
	//	PerformLineTrace();
	//} else {
	//	ServerFire();
	//}
}

bool AFPSCharacter::ServerFire_Validate()
{
	return true;
}

void AFPSCharacter::ServerFire_Implementation()
{
	PerformLineTrace();
}

void AFPSCharacter::EquipWeapon(TSubclassOf<class AWeaponBase> WeaponClass)
{
	if (false == HasAuthority())
	{
		return;
	}

	m_EquipWeapon = GetWorld()->SpawnActor<AWeaponBase>(WeaponClass);

	AWeaponBase* pWeapon = Cast<AWeaponBase>(m_EquipWeapon);
	if (pWeapon == nullptr)
	{
		return;
	}

	pWeapon->m_pOwnChar = this;

	WeaponSetOwner();

	m_EquipWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("WeaponSocket"));
}

void AFPSCharacter::WeaponSetOwner()
{
	if (IsValid(GetController()))
	{
		m_EquipWeapon->SetOwner(GetController());
		return;
	}

	FTimerManager& tm = GetWorld()->GetTimerManager();
	tm.SetTimer(WeaponSetOwnerTimer, this, &AFPSCharacter::WeaponSetOwner, 0.1f, false, 0.1f);
}

AActor* AFPSCharacter::FindNearestWeapon()
{
	TArray<AActor*> actors;
	GetCapsuleComponent()->GetOverlappingActors(actors, AWeaponBase::StaticClass());

	double nearestDist = 9999999.0f;
	AActor* pNearestActor = nullptr;
	for (AActor* pTarget : actors)
	{
		if (m_EquipWeapon == pTarget)
			continue;

		double dist = FVector::Distance(GetActorLocation(), pTarget->GetActorLocation());
		if (dist >= nearestDist)
			continue;

		nearestDist = dist;
		pNearestActor = pTarget;
	}

	return pNearestActor;
}

void AFPSCharacter::ReqPickUp_Implementation()
{
	AActor* pNearestActor = FindNearestWeapon();

	if (false == IsValid(pNearestActor))
		return;

	if (nullptr != m_EquipWeapon)
	{
		m_EquipWeapon->SetOwner(nullptr);
	}

	pNearestActor->SetOwner(GetController());

	ResPickUp(pNearestActor);
}

void AFPSCharacter::ResPickUp_Implementation(AActor* PickUpActor)
{
	if (nullptr != m_EquipWeapon)
	{
		IWeaponInterface* InterfaceObj = Cast<IWeaponInterface>(m_EquipWeapon);
		if (nullptr == InterfaceObj)
			return;

		InterfaceObj->Execute_EventDrop(m_EquipWeapon, this);
		m_EquipWeapon = nullptr;
	}

	m_EquipWeapon = PickUpActor;

	IWeaponInterface* InterfaceObj = Cast<IWeaponInterface>(m_EquipWeapon);
	if (nullptr == InterfaceObj)
		return;

	InterfaceObj->Execute_EventPickUp(m_EquipWeapon, this);
}

void AFPSCharacter::ResPressFClient_Implementation()
{
}

void AFPSCharacter::ReqTrigger_Implementation()
{
	ResTrigger();
}

void AFPSCharacter::ResTrigger_Implementation()
{
	IWeaponInterface* WeaponInterface = Cast<IWeaponInterface>(m_EquipWeapon);
	if (WeaponInterface == nullptr)
	{
		return;
	}

	WeaponInterface->Execute_EventTrigger(m_EquipWeapon);
	//EventShoot_Implementation();
}

void AFPSCharacter::ReqReload_Implementation()
{
	ResReload();
}

void AFPSCharacter::ResReload_Implementation()
{
	IWeaponInterface* InterfaceObj = Cast<IWeaponInterface>(m_EquipWeapon);
	if (nullptr == InterfaceObj)
		return;

	InterfaceObj->Execute_EventReload(m_EquipWeapon);
}

void AFPSCharacter::ReqDrop_Implementation()
{
	if (false == IsValid(m_EquipWeapon))
		return;

	m_EquipWeapon->SetOwner(nullptr);
	ResDrop();
}

void AFPSCharacter::ResDrop_Implementation()
{
	IWeaponInterface* InterfaceObj = Cast<IWeaponInterface>(m_EquipWeapon);
	if (nullptr == InterfaceObj)
		return;

	InterfaceObj->Execute_EventDrop(m_EquipWeapon, this);
	m_EquipWeapon = nullptr;
}

void AFPSCharacter::PerformLineTrace()
{
	
	//FVector EyeLocation;
	//FRotator EyeRotation;
	//GetActorEyesViewPoint(EyeLocation, EyeRotation);
	//
	//FVector TraceEnd = EyeLocation + (EyeRotation.Vector() * 10000);
	//
	//FHitResult HitResult;
	//FCollisionQueryParams QueryParams;
	//QueryParams.AddIgnoredActor(this); // 자기 자신은 무시
	//QueryParams.bTraceComplex = true;
	//
	//if (GetWorld()->LineTraceSingleByChannel(HitResult, EyeLocation, TraceEnd, ECC_Visibility, QueryParams)) {
	//	// 히트 이벤트 처리
	//	AActor* HitActor = HitResult.GetActor();
	//	// HitActor에 대한 처리 로직 추가
	//	// 데미지 떨어지는 로직 추가
	//}
	//// 라인트레이스 시각화
	//if (GetWorld())
	//{
	//	DrawDebugLine(GetWorld(),EyeLocation,TraceEnd,FColor::Red,false,1.0f, 0, 1.0f);
	//}
}
