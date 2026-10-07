using System;
using System.Collections.Generic;
using RimWorld;
using UnityEngine;
using Verse;

namespace FacialHairPlacement
{
    public static class SettingsWindow
    {
        private const float ColumnGap = 20f;
        private const float RowHeight = 30f;
        private const float SectionHeaderHeight = 28f;
        private const float RowLabelWidth = 120f;
        private const float NudgeButtonSize = 24f;
        private const float ValueLabelWidth = 60f;
        private const float Step = 0.005f;
        private const float FineStep = 0.001f;
        private const float SliderPrecision = 0.001f;

        // Re-dirtying every pawn on every slider tick is wasteful, so map pawns refresh at most this often while dragging.
        private const float MapRefreshInterval = 0.25f;

        private const float PortraitGap = 8f;
        private const float PortraitLabelHeight = 20f;

        // Points the preview camera at the head rather than the body.
        private static readonly Vector3 PreviewCameraOffset = new Vector3(0f, 0f, 0.3f);

        private static readonly Color NoteColor = new Color(0.65f, 0.65f, 0.65f);
        private static readonly Color PortraitBackground = new Color(0.12f, 0.12f, 0.12f);

        private static readonly List<Pawn> previewCandidates = new List<Pawn>();
        private static Pawn previewPawn;
        private static float previewZoom = 1.8f;

        private static bool mapRefreshPending;
        private static float lastMapRefreshTime;

        public static void Draw(Rect inRect, FacialHairPlacementSettings settings)
        {
            float leftWidth = Mathf.Floor(inRect.width * 0.58f);
            Rect leftRect = new Rect(inRect.x, inRect.y, leftWidth, inRect.height);
            Rect rightRect = new Rect(leftRect.xMax + ColumnGap, inRect.y, inRect.width - leftWidth - ColumnGap, inRect.height);

            bool changed = DrawControls(leftRect, settings);

            if (changed)
            {
                mapRefreshPending = true;
            }
            if (mapRefreshPending && Time.realtimeSinceStartup - lastMapRefreshTime >= MapRefreshInterval)
            {
                PawnGraphicsRefresher.RefreshMapPawns();
                lastMapRefreshTime = Time.realtimeSinceStartup;
                mapRefreshPending = false;
            }

            DrawPreview(rightRect, changed);
        }

        private static bool DrawControls(Rect rect, FacialHairPlacementSettings settings)
        {
            bool changed = false;
            Listing_Standard listing = new Listing_Standard();
            listing.Begin(rect);

            bool wasEnabled = settings.enabled;
            listing.CheckboxLabeled("FHP_Enabled".Translate(), ref settings.enabled, "FHP_EnabledTip".Translate());
            changed |= wasEnabled != settings.enabled;
            listing.GapLine();

            changed |= DrawSection(listing, "FHP_Front".Translate(), null,
                "FHP_Horizontal".Translate(), ref settings.southX, ref settings.southY);
            changed |= DrawSection(listing, "FHP_Side".Translate(), "FHP_SideNote".Translate(),
                "FHP_Forward".Translate(), ref settings.sideX, ref settings.sideY);
            changed |= DrawSection(listing, "FHP_Back".Translate(), "FHP_BackNote".Translate(),
                "FHP_Horizontal".Translate(), ref settings.northX, ref settings.northY);

            listing.GapLine();
            if (listing.ButtonText("FHP_ResetAll".Translate(), widthPct: 0.35f))
            {
                settings.ResetOffsets();
                changed = true;
            }
            listing.Gap(6f);
            DrawNote(listing, "FHP_Help".Translate());

            listing.End();
            return changed;
        }

        private static bool DrawSection(Listing_Standard listing, string title, string note, string xLabel, ref float x, ref float y)
        {
            bool changed = false;

            Rect header = listing.GetRect(SectionHeaderHeight);
            Text.Anchor = TextAnchor.MiddleLeft;
            Widgets.Label(header.LeftPartPixels(header.width - 80f), title);
            Text.Anchor = TextAnchor.UpperLeft;
            if (Widgets.ButtonText(header.RightPartPixels(70f).ContractedBy(0f, 2f), "FHP_Reset".Translate()))
            {
                x = 0f;
                y = 0f;
                changed = true;
            }

            if (note != null)
            {
                DrawNote(listing, note);
            }

            changed |= DrawOffsetRow(listing.GetRect(RowHeight), xLabel, ref x);
            changed |= DrawOffsetRow(listing.GetRect(RowHeight), "FHP_Vertical".Translate(), ref y);
            listing.Gap(8f);
            return changed;
        }

        // [label] [-] [========slider========] [+] [value]
        private static bool DrawOffsetRow(Rect row, string label, ref float value)
        {
            float before = value;
            float max = FacialHairPlacementSettings.MaxOffset;

            Rect labelRect = row.LeftPartPixels(RowLabelWidth);
            Rect valueRect = row.RightPartPixels(ValueLabelWidth);
            float buttonY = row.y + (row.height - NudgeButtonSize) / 2f;
            Rect minusRect = new Rect(labelRect.xMax, buttonY, NudgeButtonSize, NudgeButtonSize);
            Rect plusRect = new Rect(valueRect.x - 6f - NudgeButtonSize, buttonY, NudgeButtonSize, NudgeButtonSize);
            Rect sliderRect = new Rect(minusRect.xMax + 6f, row.y, plusRect.x - minusRect.xMax - 12f, row.height);

            Text.Anchor = TextAnchor.MiddleLeft;
            Widgets.Label(labelRect, label);
            Text.Anchor = TextAnchor.MiddleRight;
            Widgets.Label(valueRect, value.ToString("+0.000;-0.000;0.000"));
            Text.Anchor = TextAnchor.UpperLeft;

            float step = Event.current.shift ? FineStep : Step;
            if (Widgets.ButtonText(minusRect, "-"))
            {
                value -= step;
            }
            if (Widgets.ButtonText(plusRect, "+"))
            {
                value += step;
            }
            value = Widgets.HorizontalSlider(sliderRect, value, -max, max, middleAlignment: true, roundTo: SliderPrecision);
            value = Mathf.Clamp((float)Math.Round(value, 3), -max, max);

            return value != before;
        }

        private static void DrawNote(Listing_Standard listing, string text)
        {
            Text.Font = GameFont.Tiny;
            GUI.color = NoteColor;
            listing.Label(text);
            GUI.color = Color.white;
            Text.Font = GameFont.Small;
        }

        private static void DrawPreview(Rect rect, bool settingsChanged)
        {
            Widgets.DrawMenuSection(rect);
            Rect inner = rect.ContractedBy(10f);
            float curY = inner.y;

            Text.Font = GameFont.Medium;
            Widgets.Label(new Rect(inner.x, curY, inner.width, 32f), "FHP_Preview".Translate());
            Text.Font = GameFont.Small;
            curY += 36f;

            if (Current.ProgramState != ProgramState.Playing)
            {
                DrawPreviewMessage(inner, curY, "FHP_PreviewNoGame".Translate());
                return;
            }

            Pawn pawn = DrawPawnSelector(new Rect(inner.x, curY, inner.width, 28f));
            if (pawn == null)
            {
                DrawPreviewMessage(inner, curY, "FHP_PreviewNoPawns".Translate());
                return;
            }
            curY += 36f;

            if (settingsChanged)
            {
                PortraitsCache.SetDirty(pawn);
            }

            // 2x2 grid of portraits, sized to fit whichever dimension is tighter.
            float zoomRowHeight = 28f;
            float availableHeight = inner.yMax - curY - zoomRowHeight - PortraitGap;
            float cell = Mathf.Floor(Mathf.Min(
                (inner.width - PortraitGap) / 2f,
                (availableHeight - 2f * PortraitLabelHeight - PortraitGap) / 2f));
            float gridX = inner.x + (inner.width - (2f * cell + PortraitGap)) / 2f;
            float col2X = gridX + cell + PortraitGap;

            DrawPortrait(new Rect(gridX, curY, cell, cell), pawn, Rot4.South, "FHP_South".Translate());
            DrawPortrait(new Rect(col2X, curY, cell, cell), pawn, Rot4.East, "FHP_East".Translate());
            curY += cell + PortraitLabelHeight + PortraitGap;
            DrawPortrait(new Rect(gridX, curY, cell, cell), pawn, Rot4.West, "FHP_West".Translate());
            DrawPortrait(new Rect(col2X, curY, cell, cell), pawn, Rot4.North, "FHP_North".Translate());
            curY += cell + PortraitLabelHeight + PortraitGap;

            Rect zoomRow = new Rect(inner.x, curY, inner.width, zoomRowHeight);
            Text.Anchor = TextAnchor.MiddleLeft;
            Widgets.Label(zoomRow.LeftPartPixels(60f), "FHP_PreviewZoom".Translate());
            Text.Anchor = TextAnchor.UpperLeft;
            Rect zoomSlider = new Rect(zoomRow.x + 64f, zoomRow.y, zoomRow.width - 64f, zoomRow.height);
            previewZoom = Widgets.HorizontalSlider(zoomSlider, previewZoom, 1f, 3f, middleAlignment: true, roundTo: 0.1f);
        }

        // [<] name [>]  Returns the selected pawn, or null if no pawn has facial hair.
        private static Pawn DrawPawnSelector(Rect row)
        {
            CollectPreviewCandidates();
            if (previewCandidates.Count == 0)
            {
                previewPawn = null;
                return null;
            }

            int index = previewCandidates.IndexOf(previewPawn);
            if (index < 0)
            {
                index = 0;
            }

            if (Widgets.ButtonText(row.LeftPartPixels(30f), "<"))
            {
                index = (index - 1 + previewCandidates.Count) % previewCandidates.Count;
            }
            if (Widgets.ButtonText(row.RightPartPixels(30f), ">"))
            {
                index = (index + 1) % previewCandidates.Count;
            }
            previewPawn = previewCandidates[index];

            Text.Anchor = TextAnchor.MiddleCenter;
            Widgets.Label(new Rect(row.x + 34f, row.y, row.width - 68f, row.height), previewPawn.LabelShortCap);
            Text.Anchor = TextAnchor.UpperLeft;
            return previewPawn;
        }

        // Colonists first, then everyone else with facial hair.
        private static void CollectPreviewCandidates()
        {
            previewCandidates.Clear();
            foreach (Map map in Find.Maps)
            {
                foreach (Pawn pawn in map.mapPawns.AllHumanlikeSpawned)
                {
                    if (pawn.IsColonist && HasFacialHair(pawn))
                    {
                        previewCandidates.Add(pawn);
                    }
                }
            }
            foreach (Map map in Find.Maps)
            {
                foreach (Pawn pawn in map.mapPawns.AllHumanlikeSpawned)
                {
                    if (!pawn.IsColonist && HasFacialHair(pawn))
                    {
                        previewCandidates.Add(pawn);
                    }
                }
            }
        }

        private static bool HasFacialHair(Pawn pawn)
        {
            BeardDef beard = pawn.style?.beardDef;
            return beard != null && beard != BeardDefOf.NoBeard;
        }

        private static void DrawPortrait(Rect rect, Pawn pawn, Rot4 rotation, string label)
        {
            Widgets.DrawBoxSolid(rect, PortraitBackground);
            RenderTexture texture = PortraitsCache.Get(pawn, rect.size, rotation, PreviewCameraOffset, previewZoom, renderHeadgear: false);
            GUI.DrawTexture(rect, texture);
            Widgets.DrawBox(rect);

            Text.Font = GameFont.Tiny;
            Text.Anchor = TextAnchor.UpperCenter;
            Widgets.Label(new Rect(rect.x, rect.yMax + 2f, rect.width, PortraitLabelHeight), label);
            Text.Anchor = TextAnchor.UpperLeft;
            Text.Font = GameFont.Small;
        }

        private static void DrawPreviewMessage(Rect inner, float y, string message)
        {
            GUI.color = NoteColor;
            Text.Anchor = TextAnchor.UpperCenter;
            Widgets.Label(new Rect(inner.x, y, inner.width, inner.yMax - y), message);
            Text.Anchor = TextAnchor.UpperLeft;
            GUI.color = Color.white;
        }
    }
}
