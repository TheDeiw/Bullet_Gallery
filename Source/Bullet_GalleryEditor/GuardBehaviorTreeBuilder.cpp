#include "GuardBehaviorTreeBuilder.h"

#include "BTTask_Dodge.h"
#include "BTTask_FindPatrolPoint.h"
#include "BTTask_LookAround.h"
#include "BTTask_ShootTarget.h"

#include "AIGraphTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace GuardAI
{
	const FString Folder = TEXT("/Game/_MuseumProject/AI");

	/** Loads Folder/Name, or creates a new asset of class T there */
	template <typename T>
	T* LoadOrCreateAsset(const FString& Name)
	{
		const FString PackageName = Folder / Name;
		if (T* Existing = LoadObject<T>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackageName);
		T* Asset = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetPackage();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}

	/** Sets a property (also a protected one) from its text form, as the Details panel would */
	void SetProperty(UObject* Object, const TCHAR* PropertyName, const FString& Value)
	{
		FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(PropertyName) : nullptr;
		if (!Property)
		{
			UE_LOG(LogTemp, Warning, TEXT("GuardBT: %s has no property %s"), *GetNameSafe(Object), PropertyName);
			return;
		}
		Property->ImportText_InContainer(*Value, Object, Object, PPF_None);
	}

	FString KeySelector(const TCHAR* KeyName)
	{
		return FString::Printf(TEXT("(SelectedKeyName=\"%s\")"), KeyName);
	}

	UBlackboardKeyType* AddKey(UBlackboardData* Blackboard, const TCHAR* Name, TSubclassOf<UBlackboardKeyType> Type)
	{
		FBlackboardEntry Entry;
		Entry.EntryName = Name;
		Entry.KeyType = NewObject<UBlackboardKeyType>(Blackboard, Type);
		Blackboard->Keys.Add(Entry);
		return Entry.KeyType;
	}

	/** Places Behavior Tree editor nodes the same way the editor's context menu does */
	struct FGraphBuilder
	{
		UBehaviorTreeGraph* Graph = nullptr;

		UBehaviorTreeGraphNode* AddNode(TSubclassOf<UBehaviorTreeGraphNode> GraphNodeClass, UClass* NodeClass,
			UBehaviorTreeGraphNode* Parent, int32 X, int32 Y, const TCHAR* Name = nullptr)
		{
			UBehaviorTreeGraphNode* Node = NewObject<UBehaviorTreeGraphNode>(Graph, GraphNodeClass);
			Node->ClassData = FGraphNodeClassData(NodeClass, FString());
			Node->SetFlags(RF_Transactional);
			Graph->AddNode(Node, false, false);
			Node->CreateNewGuid();
			Node->PostPlacedNewNode();   // creates the runtime node (NodeInstance)
			Node->AllocateDefaultPins();
			Node->NodePosX = X;
			Node->NodePosY = Y;

			if (Name)
			{
				CastChecked<UBTNode>(Node->NodeInstance)->NodeName = Name;
			}
			if (Parent)
			{
				Parent->GetOutputPin()->MakeLinkTo(Node->GetInputPin());
			}
			return Node;
		}

		UBehaviorTreeGraphNode* Composite(UClass* NodeClass, UBehaviorTreeGraphNode* Parent, int32 X, int32 Y, const TCHAR* Name)
		{
			return AddNode(UBehaviorTreeGraphNode_Composite::StaticClass(), NodeClass, Parent, X, Y, Name);
		}

		UBTNode* Task(UClass* NodeClass, UBehaviorTreeGraphNode* Parent, int32 X, int32 Y, const TCHAR* Name = nullptr)
		{
			return CastChecked<UBTNode>(AddNode(UBehaviorTreeGraphNode_Task::StaticClass(), NodeClass, Parent, X, Y, Name)->NodeInstance);
		}

		/** Blackboard decorator "Key is (not) set" with the given observer abort mode */
		void BlackboardDecorator(UBehaviorTreeGraphNode* Node, const TCHAR* Name, const TCHAR* Key, bool bIsSet, const TCHAR* AbortMode)
		{
			UBehaviorTreeGraphNode_Decorator* DecoratorNode = NewObject<UBehaviorTreeGraphNode_Decorator>(Graph);
			DecoratorNode->ClassData = FGraphNodeClassData(UBTDecorator_Blackboard::StaticClass(), FString());
			Node->AddSubNode(DecoratorNode, Graph);

			UBTDecorator* Decorator = CastChecked<UBTDecorator>(DecoratorNode->NodeInstance);
			Decorator->NodeName = Name;
			SetProperty(Decorator, TEXT("BlackboardKey"), KeySelector(Key));
			SetProperty(Decorator, TEXT("BasicOperation"), bIsSet ? TEXT("Set") : TEXT("NotSet"));
			// The decorator tests OperationType; the Details panel copies it from BasicOperation
			SetProperty(Decorator, TEXT("OperationType"), bIsSet ? TEXT("0") : TEXT("1"));
			SetProperty(Decorator, TEXT("FlowAbortMode"), AbortMode);
		}
	};
}

FString UGuardBehaviorTreeBuilder::BuildGuardAssets()
{
	using namespace GuardAI;

	// Blueprint tasks made in the editor (Lab 5 guide, part 7.2)
	UClass* SetSpeedTask = LoadObject<UClass>(nullptr, *(Folder + TEXT("/BTT_SetSpeed.BTT_SetSpeed_C")));
	UClass* MoveToRangeTask = LoadObject<UClass>(nullptr, *(Folder + TEXT("/BTT_MoveToEffectiveRange.BTT_MoveToEffectiveRange_C")));
	if (!SetSpeedTask || !MoveToRangeTask)
	{
		return TEXT("Error: create BTT_SetSpeed and BTT_MoveToEffectiveRange in ") + Folder + TEXT(" first");
	}

	// ---------- Blackboard ----------

	UBlackboardData* Blackboard = LoadOrCreateAsset<UBlackboardData>(TEXT("BB_MuseumGuard"));
	Blackboard->Modify();
	Blackboard->Keys.Reset();
	Cast<UBlackboardKeyType_Object>(AddKey(Blackboard, TEXT("SelfActor"), UBlackboardKeyType_Object::StaticClass()))->BaseClass = AActor::StaticClass();
	AddKey(Blackboard, TEXT("PatrolTarget"), UBlackboardKeyType_Vector::StaticClass());
	AddKey(Blackboard, TEXT("HasLineOfSight"), UBlackboardKeyType_Bool::StaticClass());
	Cast<UBlackboardKeyType_Object>(AddKey(Blackboard, TEXT("DetectedEnemy"), UBlackboardKeyType_Object::StaticClass()))->BaseClass = AActor::StaticClass();
	AddKey(Blackboard, TEXT("LastKnownLocation"), UBlackboardKeyType_Vector::StaticClass());
	AddKey(Blackboard, TEXT("IncomingShot"), UBlackboardKeyType_Bool::StaticClass());
	AddKey(Blackboard, TEXT("ShotOrigin"), UBlackboardKeyType_Vector::StaticClass());
	Blackboard->PostEditChange();

	// ---------- Behavior Tree with a fresh editor graph ----------

	UBehaviorTree* Tree = LoadOrCreateAsset<UBehaviorTree>(TEXT("BT_MuseumGuard"));
	Tree->Modify();
	Tree->BlackboardAsset = Blackboard;

	// A rebuild replaces the old graph: move it out of the asset so its name is free
	if (Tree->BTGraph)
	{
		Tree->BTGraph->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Tree->BTGraph = nullptr;
	}
	// ...and forget the old runtime tree, otherwise the graph spawns editor nodes for it again
	Tree->RootNode = nullptr;
	Tree->RootDecorators.Reset();
	Tree->RootDecoratorOps.Reset();

	UBehaviorTreeGraph* Graph = CastChecked<UBehaviorTreeGraph>(FBlueprintEditorUtils::CreateNewGraph(
		Tree, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass()));
	Tree->BTGraph = Graph;
	Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
	Graph->OnCreated();

	UBehaviorTreeGraphNode_Root* Root = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		Root = Root ? Root : Cast<UBehaviorTreeGraphNode_Root>(Node);
	}
	check(Root);
	Root->BlackboardAsset = Blackboard;

	FGraphBuilder B{ Graph };

	// Children run left to right: the leftmost branch has the highest priority
	UBehaviorTreeGraphNode* Brain = B.Composite(UBTComposite_Selector::StaticClass(), Root, 0, 160, TEXT("TasksSelector"));

	// 1. Dodge a shot (top priority: interrupts everything else)
	UBehaviorTreeGraphNode* DodgeBranch = B.Composite(UBTComposite_Sequence::StaticClass(), Brain, -1700, 360, TEXT("Dodge Shot"));
	B.BlackboardDecorator(DodgeBranch, TEXT("Is shot at?"), TEXT("IncomingShot"), true, TEXT("LowerPriority"));
	B.Task(UBTTask_Dodge::StaticClass(), DodgeBranch, -1700, 600);

	// 2. The player is in sight: run to the weapon range and shoot
	UBehaviorTreeGraphNode* Chase = B.Composite(UBTComposite_Sequence::StaticClass(), Brain, -750, 360, TEXT("Chase And Shoot"));
	B.BlackboardDecorator(Chase, TEXT("Does have line of sight?"), TEXT("HasLineOfSight"), true, TEXT("LowerPriority"));
	SetProperty(B.Task(SetSpeedTask, Chase, -1300, 600, TEXT("Run")), TEXT("bRun"), TEXT("True"));
	SetProperty(B.Task(MoveToRangeTask, Chase, -1020, 600), TEXT("Enemy"), KeySelector(TEXT("DetectedEnemy")));
	SetProperty(B.Task(UBTTask_ShootTarget::StaticClass(), Chase, -640, 600), TEXT("BlackboardKey"), KeySelector(TEXT("DetectedEnemy")));
	SetProperty(B.Task(UBTTask_Wait::StaticClass(), Chase, -340, 600, TEXT("Reload Pause")), TEXT("WaitTime"), TEXT("(DefaultValue=1.0)"));

	// 3. The player is not in sight: check where they were seen, or patrol
	UBehaviorTreeGraphNode* NotInSight = B.Composite(UBTComposite_Selector::StaticClass(), Brain, 800, 360, TEXT("Enemy Not In Sight"));
	B.BlackboardDecorator(NotInSight, TEXT("Has lost line of sight?"), TEXT("HasLineOfSight"), false, TEXT("None"));

	UBehaviorTreeGraphNode* CheckLast = B.Composite(UBTComposite_Sequence::StaticClass(), NotInSight, 250, 600, TEXT("Check Last Known Location"));
	B.BlackboardDecorator(CheckLast, TEXT("Has seen enemy?"), TEXT("DetectedEnemy"), true, TEXT("None"));
	SetProperty(B.Task(SetSpeedTask, CheckLast, -80, 840, TEXT("Walk")), TEXT("bRun"), TEXT("False"));
	SetProperty(B.Task(UBTTask_MoveTo::StaticClass(), CheckLast, 200, 840), TEXT("BlackboardKey"), KeySelector(TEXT("LastKnownLocation")));
	B.Task(UBTTask_LookAround::StaticClass(), CheckLast, 480, 840);

	UBehaviorTreeGraphNode* Patrol = B.Composite(UBTComposite_Sequence::StaticClass(), NotInSight, 1350, 600, TEXT("Patrol"));
	SetProperty(B.Task(SetSpeedTask, Patrol, 800, 840, TEXT("Walk")), TEXT("bRun"), TEXT("False"));
	B.Task(UBTTask_FindPatrolPoint::StaticClass(), Patrol, 1060, 840);
	SetProperty(B.Task(UBTTask_MoveTo::StaticClass(), Patrol, 1360, 840), TEXT("BlackboardKey"), KeySelector(TEXT("PatrolTarget")));
	B.Task(UBTTask_LookAround::StaticClass(), Patrol, 1640, 840);
	UBTNode* Wait = B.Task(UBTTask_Wait::StaticClass(), Patrol, 1900, 840);
	SetProperty(Wait, TEXT("WaitTime"), TEXT("(DefaultValue=1.5)"));
	SetProperty(Wait, TEXT("RandomDeviation"), TEXT("(DefaultValue=0.5)"));

	// Resolve the Blackboard keys of every node and write the graph into the runtime tree
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node))
		{
			TArray<UAIGraphNode*> AllNodes = { BTNode };
			AllNodes.Append(BTNode->SubNodes);
			for (UAIGraphNode* Each : AllNodes)
			{
				if (UBTNode* Instance = Each ? Cast<UBTNode>(Each->NodeInstance) : nullptr)
				{
					Instance->InitializeFromAsset(*Tree);
				}
			}
		}
	}
	Graph->UpdateAsset();
	Graph->OnSave();

	const bool bSaved = SaveAsset(Blackboard) && SaveAsset(Tree);
	return FString::Printf(TEXT("%s: %d keys, %d graph nodes"), bSaved ? TEXT("Saved") : TEXT("Error: not saved"),
		Blackboard->Keys.Num(), Graph->Nodes.Num());
}
