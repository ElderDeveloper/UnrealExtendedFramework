// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimationTags.h"

DEFINE_LOG_CATEGORY(LogEGAnimation);

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGAnim_Footstep, "ExtendedGameplay.Anim.Footstep", "A foot touched the ground (UEGFootstepNotify).");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGAnim_Mesh_Body, "ExtendedGameplay.Anim.Mesh.Body", "The full-body mesh montages play on.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGAnim_Mesh_FirstPerson, "ExtendedGameplay.Anim.Mesh.FirstPerson", "First-person arms, when the character has them.");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGMelee_Window, "ExtendedGameplay.Melee.Window", "Root of melee hit windows; UEGMeleeTraceComponent shapes are tagged under it.");

UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_GetIn, "ExtendedGameplay.Anim.Cover.GetIn");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_GetIn_Crouched, "ExtendedGameplay.Anim.Cover.GetIn.Crouched");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_GetOut, "ExtendedGameplay.Anim.Cover.GetOut");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_GetOut_Crouched, "ExtendedGameplay.Anim.Cover.GetOut.Crouched");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_TurnLeft, "ExtendedGameplay.Anim.Cover.TurnLeft");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_TurnLeft_Crouched, "ExtendedGameplay.Anim.Cover.TurnLeft.Crouched");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_TurnRight, "ExtendedGameplay.Anim.Cover.TurnRight");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_Cover_TurnRight_Crouched, "ExtendedGameplay.Anim.Cover.TurnRight.Crouched");

UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_State_Cover, "ExtendedGameplay.Anim.State.Cover");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_State_Cover_Crouched, "ExtendedGameplay.Anim.State.Cover.Crouched");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_State_Cover_Moving, "ExtendedGameplay.Anim.State.Cover.Moving");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_State_Cover_Left, "ExtendedGameplay.Anim.State.Cover.Left");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_State_Cover_Right, "ExtendedGameplay.Anim.State.Cover.Right");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGAnim_State_Cover_Montage, "ExtendedGameplay.Anim.State.Cover.Montage");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGTurnInPlace_Mode_Movement, "ExtendedGameplay.TurnInPlace.Mode.Movement", "Turn angles used while orienting to movement.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGTurnInPlace_Mode_Strafe, "ExtendedGameplay.TurnInPlace.Mode.Strafe", "Turn angles used while strafing (facing the control rotation).");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGTurnInPlace_Pause, "ExtendedGameplay.TurnInPlace.Pause", "Anim event window: turn in place is paused while it is open.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(TAG_EGTurnInPlace_Lock, "ExtendedGameplay.TurnInPlace.Lock", "Anim event window: turn in place is locked while it is open.");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGTurnInPlace_State_Turning, "ExtendedGameplay.TurnInPlace.State.Turning");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGTurnInPlace_State_Locked, "ExtendedGameplay.TurnInPlace.State.Locked");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGTurnInPlace_State_Paused, "ExtendedGameplay.TurnInPlace.State.Paused");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGTurnInPlace_State_Left, "ExtendedGameplay.TurnInPlace.State.Left");
UE_DEFINE_GAMEPLAY_TAG(TAG_EGTurnInPlace_State_Right, "ExtendedGameplay.TurnInPlace.State.Right");
