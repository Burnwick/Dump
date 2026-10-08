using UnrealBuildTool;
using System.Collections.Generic;

public class HeavyShooterEditorTarget : TargetRules
{
	public HeavyShooterEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("HeavyShooter");
	}
}
