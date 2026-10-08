using UnrealBuildTool;

public class PrimFrontEditorTarget : TargetRules
{
	public PrimFrontEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("PrimFront");
	}
}
