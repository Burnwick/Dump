using RimWorld;
using Verse;

namespace FacialHairPlacement
{
    // Pawn drawings and portraits are cached, so they must be marked dirty to pick up new offsets.
    public static class PawnGraphicsRefresher
    {
        public static void RefreshMapPawns()
        {
            if (Current.ProgramState != ProgramState.Playing)
            {
                return;
            }

            foreach (Map map in Find.Maps)
            {
                foreach (Pawn pawn in map.mapPawns.AllHumanlikeSpawned)
                {
                    pawn.Drawer?.renderer?.SetAllGraphicsDirty();
                    PortraitsCache.SetDirty(pawn);
                }
            }
        }

        // Also clears portraits of pawns that aren't on a map (caravans, travelling pods).
        public static void RefreshAll()
        {
            if (Current.ProgramState != ProgramState.Playing)
            {
                return;
            }

            RefreshMapPawns();
            PortraitsCache.Clear();
        }
    }
}
