#include "Lab6AssetBuilder.h"

#include "GuardAnimInstance.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace Lab6
{
	const FString Folder = TEXT("/Game/_MuseumProject/Lab6");

	template <typename T>
	T* Find(const FString& Name)
	{
		return LoadObject<T>(nullptr, *(Folder / Name + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	template <typename T>
	T* Create(const FString& Name)
	{
		UPackage* Package = CreatePackage(*(Folder / Name));
		T* Asset = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetPackage();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}

	FString BuildReloadMontage(float LoopStart, float LoopEnd, float NotifyLead)
	{
		UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload"));
		UClass* NotifyClass = FindObject<UClass>(nullptr, TEXT("/Script/AnimGraphRuntime.AnimNotify_PlayMontageNotify"));
		if (!Source || !NotifyClass) { return TEXT("montage: MM_Pistol_Reload or AnimNotify_PlayMontageNotify not found"); }

		const float Length = Source->GetPlayLength();
		LoopStart = FMath::Clamp(LoopStart, 0.05f, Length - 0.1f);
		LoopEnd = FMath::Clamp(LoopEnd, LoopStart + 0.05f, Length - 0.05f);

		UAnimMontage* Montage = Find<UAnimMontage>(TEXT("AM_Player_Reload"));
		if (!Montage) { Montage = Create<UAnimMontage>(TEXT("AM_Player_Reload")); }
		Montage->Modify();
		Montage->SetSkeleton(Source->GetSkeleton());

		// One DefaultSlot track (the montage constructor adds it) with three back-to-back pieces of the
		// same sequence, so the montage time equals the time in MM_Pistol_Reload
		Montage->SlotAnimTracks.SetNum(1);
		FSlotAnimationTrack& Track = Montage->SlotAnimTracks[0];
		Track.SlotName = FAnimSlotGroup::DefaultSlotName;
		Track.AnimTrack.AnimSegments.Reset();
		const float Cuts[] = { 0.f, LoopStart, LoopEnd, Length };
		for (int32 i = 0; i < 3; ++i)
		{
			FAnimSegment Segment;
			Segment.SetAnimReference(Source);
			Segment.AnimStartTime = Cuts[i];
			Segment.AnimEndTime = Cuts[i + 1];
			Segment.StartPos = Cuts[i];
			Track.AnimTrack.AnimSegments.Add(Segment);
		}
		static_cast<UAnimCompositeBase*>(Montage)->SetCompositeLength(Length);

		// Sections: Start -> Loop -> Loop -> ... ; the character switches Loop -> End when the magazine is full
		const FName Names[] = { TEXT("ReloadStart"), TEXT("ReloadLoop"), TEXT("ReloadEnd") };
		const FName Next[] = { Names[1], Names[1], NAME_None };
		Montage->CompositeSections.Reset();
		for (int32 i = 0; i < 3; ++i)
		{
			Montage->AddAnimCompositeSection(Names[i], Cuts[i]);
		}
		for (int32 i = 0; i < 3; ++i)
		{
			Montage->CompositeSections[Montage->GetSectionIndex(Names[i])].NextSectionName = Next[i];
		}

		// Montage Notify near the end of the loop: every pass through the loop inserts a few rounds
		const FName RoundsNotify = TEXT("RoundsInserted");
		const float NotifyTime = FMath::Max(LoopStart, LoopEnd - NotifyLead);
		UAnimNotify* Notify = NewObject<UAnimNotify>(Montage, NotifyClass, NAME_None, RF_Transactional);
		FindFProperty<FNameProperty>(NotifyClass, TEXT("NotifyName"))->SetPropertyValue_InContainer(Notify, RoundsNotify);

		Montage->Notifies.Reset();
		Montage->InitializeNotifyTrack();
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.NotifyName = RoundsNotify;
		Event.Notify = Notify;
		Event.Link(Montage, NotifyTime);
		Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(NotifyTime));
		Event.MontageTickType = EMontageNotifyTickType::BranchingPoint;
		Event.TrackIndex = 0;

		static_cast<UAnimSequenceBase*>(Montage)->RefreshCacheData();
		Montage->PostEditChange();
		const bool bSaved = Save(Montage);
		return FString::Printf(TEXT("AM_Player_Reload %s (cuts 0 / %.2f / %.2f / %.2f s, notify at %.2f s)"),
			bSaved ? TEXT("saved") : TEXT("NOT saved"), LoopStart, LoopEnd, Length, NotifyTime);
	}

	FString CreateGuardAnimBlueprint()
	{
		if (Find<UAnimBlueprint>(TEXT("ABP_Guard"))) { return TEXT("ABP_Guard exists"); }

		const UAnimSequence* Reference = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Reload.MM_Pistol_Reload"));
		if (!Reference) { return TEXT("ABP_Guard: mannequin skeleton not found"); }

		UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
		Factory->TargetSkeleton = Reference->GetSkeleton();
		Factory->ParentClass = UGuardAnimInstance::StaticClass();
		Factory->BlueprintType = BPTYPE_Normal;

		const FString Name = TEXT("ABP_Guard");
		UPackage* Package = CreatePackage(*(Folder / Name));
		UObject* Blueprint = Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package, *Name,
			RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn);
		if (!Blueprint) { return TEXT("ABP_Guard: factory failed"); }
		FAssetRegistryModule::AssetCreated(Blueprint);
		return Save(Blueprint) ? TEXT("ABP_Guard created") : TEXT("ABP_Guard NOT saved");
	}

	FString CreateWeatherCollection()
	{
		if (Find<UMaterialParameterCollection>(TEXT("MPC_Weather"))) { return TEXT("MPC_Weather exists"); }

		UMaterialParameterCollection* Collection = Create<UMaterialParameterCollection>(TEXT("MPC_Weather"));
		const TPair<const TCHAR*, float> Parameters[] = { { TEXT("WindStrength"), 0.15f }, { TEXT("WindSpeed"), 1.f }, { TEXT("RainAmount"), 0.f } };
		for (const TPair<const TCHAR*, float>& Parameter : Parameters)
		{
			FCollectionScalarParameter& Scalar = Collection->ScalarParameters.AddDefaulted_GetRef();
			Scalar.ParameterName = Parameter.Key;
			Scalar.DefaultValue = Parameter.Value;
		}
		Collection->PostEditChange();
		return Save(Collection) ? TEXT("MPC_Weather created") : TEXT("MPC_Weather NOT saved");
	}
}

FString ULab6AssetBuilder::BuildAll() const
{
	return Lab6::BuildReloadMontage(LoopStartTime, LoopEndTime, NotifyBeforeLoopEnd)
		+ TEXT("; ") + Lab6::CreateGuardAnimBlueprint()
		+ TEXT("; ") + Lab6::CreateWeatherCollection();
}
