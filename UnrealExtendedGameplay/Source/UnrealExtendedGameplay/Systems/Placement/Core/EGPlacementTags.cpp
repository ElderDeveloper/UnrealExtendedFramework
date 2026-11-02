// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementTags.h"

DEFINE_LOG_CATEGORY(LogEGPlacement);

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason, "Extended.Gameplay.Placement.Reason", "Root of placement refusal reasons.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_NotConfigured, "Extended.Gameplay.Placement.Reason.NotConfigured", "A profile, solver, rule list or commit handler is missing.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_InvalidInput, "Extended.Gameplay.Placement.Reason.InvalidInput", "The request is malformed: no world, an empty shape, a NaN pose.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_UnsupportedTransform, "Extended.Gameplay.Placement.Reason.UnsupportedTransform", "Only positive uniform scale is supported.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_NoSurface, "Extended.Gameplay.Placement.Reason.NoSurface", "Nothing to rest on.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_SurfaceOrientation, "Extended.Gameplay.Placement.Reason.SurfaceOrientation", "The object does not go on floors, walls or ceilings of this kind.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_TooSteep, "Extended.Gameplay.Placement.Reason.TooSteep", "The surface is steeper than the limit.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_SurfaceTag, "Extended.Gameplay.Placement.Reason.SurfaceTag", "The surface lacks a required tag or has a refused one.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_Blocked, "Extended.Gameplay.Placement.Reason.Blocked", "Something is in the way.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_Overhang, "Extended.Gameplay.Placement.Reason.Overhang", "Too little of the footprint rests on support.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_OutsideRegion, "Extended.Gameplay.Placement.Reason.OutsideRegion", "The footprint leaves the required regions.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_InExclusion, "Extended.Gameplay.Placement.Reason.InExclusion", "The footprint touches an exclusion region.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_OutOfReach, "Extended.Gameplay.Placement.Reason.OutOfReach", "Too far from, or too close to, the requester.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_NoLineOfSight, "Extended.Gameplay.Placement.Reason.NoLineOfSight", "The requester cannot see the spot.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_SupportRejected, "Extended.Gameplay.Placement.Reason.SupportRejected", "The object may not rest on this support.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_AnchorUnavailable, "Extended.Gameplay.Placement.Reason.AnchorUnavailable", "No compatible free anchor.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_ClaimMismatch, "Extended.Gameplay.Placement.Reason.ClaimMismatch", "What the requester claimed differs from what was measured.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_Stale, "Extended.Gameplay.Placement.Reason.Stale", "Something the requester's answer depended on has changed since.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_NoAuthority, "Extended.Gameplay.Placement.Reason.NoAuthority", "A built-in commit handler needs authority; route networked clients through the project's command.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_Pending, "Extended.Gameplay.Placement.Reason.Pending", "Data needed for an answer is not available yet.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_Pending_Region, "Extended.Gameplay.Placement.Reason.Pending.Region", "The area's regions may not be loaded.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_Pending_Asset, "Extended.Gameplay.Placement.Reason.Pending.Asset", "An asset is still loading.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Reason_Pending_Data, "Extended.Gameplay.Placement.Reason.Pending.Data", "Project data a rule needs is not available yet.");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Event, "Extended.Gameplay.Placement.Event", "Root of placement session messages.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Event_SessionStarted, "Extended.Gameplay.Placement.Event.SessionStarted", "A placement session began previewing a selection.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Event_CommitDispatched, "Extended.Gameplay.Placement.Event.CommitDispatched", "A commit handler accepted a commit.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Event_Outcome, "Extended.Gameplay.Placement.Event.Outcome", "A commit reported its outcome.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Event_Cancelled, "Extended.Gameplay.Placement.Event.Cancelled", "A placement session was cancelled.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGPlacement_Event_SessionEnded, "Extended.Gameplay.Placement.Event.SessionEnded", "A placement session ended, for any reason.");
