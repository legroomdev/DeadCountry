using UnrealBuildTool;

public class DeadCountry : ModuleRules
{
	public DeadCountry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"NetCore",
			"CoreOnline",
			"UMG",
			"Slate",
			"SlateCore"
		});

		// Lets code include e.g. "Inventory/DCInventoryComponent.h"
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
