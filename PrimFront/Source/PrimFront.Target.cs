using UnrealBuildTool;

public class PrimFrontTarget : TargetRules
{
	public PrimFrontTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PrimFront");
	}
}
