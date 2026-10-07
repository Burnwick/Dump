using System.Reflection;
using HarmonyLib;
using UnityEngine;
using Verse;

namespace FacialHairPlacement
{
    public class FacialHairPlacementMod : Mod
    {
        public const string HarmonyId = "burnwick.facialhairplacement";

        public static FacialHairPlacementSettings Settings { get; private set; }

        public FacialHairPlacementMod(ModContentPack content) : base(content)
        {
            Settings = GetSettings<FacialHairPlacementSettings>();
            new Harmony(HarmonyId).PatchAll(Assembly.GetExecutingAssembly());
        }

        public override string SettingsCategory() => "FHP_SettingsCategory".Translate();

        public override void DoSettingsWindowContents(Rect inRect) => SettingsWindow.Draw(inRect, Settings);

        // Called when the settings window closes.
        public override void WriteSettings()
        {
            base.WriteSettings();
            PawnGraphicsRefresher.RefreshAll();
        }
    }
}
