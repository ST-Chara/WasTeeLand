/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/demo.h>
#include <engine/engine.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <generated/protocol.h>
#include <generated/client_data.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/render.h>

#include <game/client/components/flow.h>
#include <game/client/components/skins.h>
#include <game/client/components/effects.h>
#include <game/client/components/sounds.h>
#include <game/client/components/controls.h>

#include "players.h"

void CPlayers::RenderHook(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CTeeRenderInfo *pRenderInfo,
	int ClientID) const
{
	CNetObj_Character Prev = *pPrevChar;
	CNetObj_Character Player = *pPlayerChar;
	CTeeRenderInfo RenderInfo = *pRenderInfo;

	float IntraTick = Client()->IntraGameTick();

	// set size
	RenderInfo.m_Size = 64.0f;

	if (m_pClient->ShouldUsePredicted() && m_pClient->ShouldUsePredictedChar(ClientID))
	{
		m_pClient->UsePredictedChar(&Prev, &Player, &IntraTick, ClientID);
	}

	vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);

	// draw hook
	if (Prev.m_HookState > 0 && Player.m_HookState > 0)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();

		vec2 HookPos;

		if (Player.m_HookedPlayer != -1 && m_pClient->m_Snap.m_aCharacters[Player.m_HookedPlayer].m_Active)
		{
			// `HookedPlayer != -1` means that a player is being hooked
			bool Predicted = m_pClient->ShouldUsePredicted() && m_pClient->ShouldUsePredictedChar(Player.m_HookedPlayer);
			HookPos = m_pClient->GetCharPos(Player.m_HookedPlayer, Predicted);
		}
		else
		{
			// The hook is in the air, on a hookable tile or the hooked player is out of range
			HookPos = mix(vec2(Prev.m_HookX, Prev.m_HookY), vec2(Player.m_HookX, Player.m_HookY), IntraTick);
		}

		const float HookDistance = distance(Position, HookPos);
		vec2 Dir = normalize(Position - HookPos);

		Graphics()->QuadsSetRotation(angle(Dir) + pi);

		// render head
		RenderTools()->SelectSprite(SPRITE_HOOK_HEAD);
		IGraphics::CQuadItem QuadItem(HookPos.x, HookPos.y, 24, 16);
		Graphics()->QuadsDraw(&QuadItem, 1);

		// render chain
		RenderTools()->SelectSprite(SPRITE_HOOK_CHAIN);
		IGraphics::CQuadItem aArray[1024];
		int i = 0;
		for (float f = 16; f < HookDistance && i < 1024; f += 16, i++)
		{
			vec2 p = HookPos + Dir * f;
			aArray[i] = IGraphics::CQuadItem(p.x, p.y, 16, 16);
		}

		Graphics()->QuadsDraw(aArray, i);
		Graphics()->QuadsSetRotation(0);
		Graphics()->QuadsEnd();

		RenderTools()->RenderTeeHand(&RenderInfo, Position, normalize(HookPos - Position), -pi / 2, vec2(20, 0));
	}
}

void CPlayers::RenderPlayer(
	const CNetObj_Character *pPrevChar,
	const CNetObj_Character *pPlayerChar,
	const CNetObj_PlayerInfo *pPlayerInfo,
	const CTeeRenderInfo *pRenderInfo,
	int ClientID) const
{
	CNetObj_Character Prev = *pPrevChar;
	CNetObj_Character Player = *pPlayerChar;
	CTeeRenderInfo RenderInfo = *pRenderInfo;

	// set size
	RenderInfo.m_Size = 64.0f;

	float IntraTick = Client()->IntraGameTick();

	if (Prev.m_Angle < pi * -128 && Player.m_Angle > pi * 128)
		Prev.m_Angle += 2 * pi * 256;
	else if (Prev.m_Angle > pi * 128 && Player.m_Angle < pi * -128)
		Player.m_Angle += 2 * pi * 256;
	float Angle = mix((float)Prev.m_Angle, (float)Player.m_Angle, IntraTick) / 256.0f;

	if (m_pClient->m_LocalClientID == ClientID && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		// just use the direct input if it's local player we are rendering
		Angle = angle(m_pClient->m_pControls->m_MousePos);
	}

	if (m_pClient->ShouldUsePredicted() && m_pClient->ShouldUsePredictedChar(ClientID))
	{
		m_pClient->UsePredictedChar(&Prev, &Player, &IntraTick, ClientID);
	}
	const bool Paused = m_pClient->IsWorldPaused() || m_pClient->IsDemoPlaybackPaused();

	vec2 Direction = direction(Angle);
	vec2 Position = mix(vec2(Prev.m_X, Prev.m_Y), vec2(Player.m_X, Player.m_Y), IntraTick);
	vec2 Vel = mix(vec2(Prev.m_VelX / 256.0f, Prev.m_VelY / 256.0f), vec2(Player.m_VelX / 256.0f, Player.m_VelY / 256.0f), IntraTick);

	m_pClient->m_pFlow->Add(Position, Vel * 100.0f, 10.0f);

	RenderInfo.m_GotAirJump = Player.m_Jumped & 2 ? 0 : 1;

	bool Stationary = Player.m_VelX <= 1 && Player.m_VelX >= -1;
	bool InAir = !Collision()->CheckPoint(Player.m_X, Player.m_Y + 16);
	bool WantOtherDir = (Player.m_Direction == -1 && Vel.x > 0) || (Player.m_Direction == 1 && Vel.x < 0);

	// evaluate animation
	const float WalkTimeMagic = 100.0f;
	float WalkTime =
		((Position.x >= 0)
			 ? fmod(Position.x, WalkTimeMagic)
			 : WalkTimeMagic - fmod(-Position.x, WalkTimeMagic)) /
		WalkTimeMagic;
	CAnimState State;
	State.Set(&g_pData->m_aAnimations[ANIM_BASE], 0);

	if (InAir)
		State.Add(&g_pData->m_aAnimations[ANIM_INAIR], 0, 1.0f); // TODO: some sort of time here
	else if (Stationary)
		State.Add(&g_pData->m_aAnimations[ANIM_IDLE], 0, 1.0f); // TODO: some sort of time here
	else if (!WantOtherDir)
		State.Add(&g_pData->m_aAnimations[ANIM_WALK], WalkTime, 1.0f);

	static float s_LastGameTickTime = Client()->GameTickTime();
	static float s_LastIntraTick = IntraTick;
	static float s_TimeUntilAnimationFrame = 1.0f;
	bool UpdateSingleAnimationFrame = false;
	if (!Paused)
	{
		s_LastGameTickTime = Client()->GameTickTime();
		s_LastIntraTick = IntraTick;
		s_TimeUntilAnimationFrame -= m_pClient->GetAnimationPlaybackSpeed();
		if (s_TimeUntilAnimationFrame <= 0.0f)
		{
			s_TimeUntilAnimationFrame += 1.0f;
			UpdateSingleAnimationFrame = true;
		}
	}

	{
		float ct = (Client()->PrevGameTick() - Player.m_AttackTick) / (float)SERVER_TICK_SPEED + s_LastGameTickTime;
		if (Player.m_Weapon == WEAPON_HAMMER)
			State.Add(&g_pData->m_aAnimations[ANIM_HAMMER_SWING], clamp(ct * 5.0f, 0.0f, 1.0f), 1.0f);
		if (Player.m_Weapon == WEAPON_NINJA)
			State.Add(&g_pData->m_aAnimations[ANIM_NINJA_SWING], clamp(ct * 2.0f, 0.0f, 1.0f), 1.0f);
		if (Player.m_Weapon == WEAPON_SWORD)
			State.Add(&g_pData->m_aAnimations[ANIM_SWORD_RUSH], clamp(ct * 5.0f, 0.0f, 1.0f), 1.0f);
		if (Player.m_Weapon == WEAPON_SPARK)
			State.Add(&g_pData->m_aAnimations[ANIM_SPARK_FIRE], clamp(ct * 5.0f, 0.0f, 1.0f), 1.0f);
		if (Player.m_Weapon == WEAPON_BLOCK)
			State.Add(&g_pData->m_aAnimations[ANIM_SWORD_RUSH], clamp(ct * 2.0f, 0.0f, 1.0f), 1.0f);
	}
	// do skidding
	if (!InAir && WantOtherDir && length(Vel * 50) > 500.0f)
	{
		static int64 s_SkidSoundTime = 0;
		if (time_get() - s_SkidSoundTime > time_freq() / 10)
		{
			m_pClient->m_pSounds->PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_SKID, 0.25f, Position);
			s_SkidSoundTime = time_get();
		}

		m_pClient->m_pEffects->SkidTrail(
			Position + vec2(-Player.m_Direction * 6, 12),
			vec2(-Player.m_Direction * 100 * length(Vel), -50));
	}

	// draw gun
	if (Player.m_Weapon >= 0)
	{
		int ImageToBeUse = IMAGE_GAME;
		switch (Player.m_Weapon)
		{
		case WEAPON_SWORD:
			ImageToBeUse = IMAGE_SWORD;
			break;

		case WEAPON_SPARK:
			ImageToBeUse = IMAGE_SPARK;
			break;

		case WEAPON_SCYTHE:
			ImageToBeUse = IMAGE_SCYTHE;
			break;

		case WEAPON_BLOCK:
			ImageToBeUse = IMAGE_BLOCKS;
			break;

		default:
			ImageToBeUse = IMAGE_GAME;
			break;
		}
		Graphics()->TextureSet(g_pData->m_aImages[ImageToBeUse].m_Id);

		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(State.GetAttach()->m_Angle * pi * 2 + Angle);

		// normal weapons
		const int Weapon = clamp(Player.m_Weapon, 0, NUM_WEAPONS - 1);
		RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[Weapon].m_pSpriteBody, Direction.x < 0 ? SPRITE_FLAG_FLIP_Y : 0);

		vec2 p;
		switch (Player.m_Weapon)
		{
		case WEAPON_NINJA:
			p = Position;
			p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;

			if (Direction.x < 0)
			{
				Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle * pi * 2);
				p.x -= g_pData->m_Weapons.m_aId[Weapon].m_Offsetx;
				m_pClient->m_pEffects->PowerupShine(p + vec2(32, 0), vec2(32, 12));
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle * pi * 2);
				m_pClient->m_pEffects->PowerupShine(p - m_pClient->GetCharPos(m_pClient->m_LocalClientID), vec2(32, 12));
			}
			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize);

			// HADOKEN
			if ((Client()->GameTick() - Player.m_AttackTick) <= (SERVER_TICK_SPEED / 6) && g_pData->m_Weapons.m_aId[Weapon].m_NumSpriteMuzzles)
			{
				const int IteX = random_int() % g_pData->m_Weapons.m_aId[Weapon].m_NumSpriteMuzzles;
				static int s_LastIteX = IteX;
				if (UpdateSingleAnimationFrame)
					s_LastIteX = IteX;

				if (g_pData->m_Weapons.m_aId[Weapon].m_aSpriteMuzzles[s_LastIteX])
				{
					const vec2 Dir = normalize(vec2(pPlayerChar->m_X, pPlayerChar->m_Y) - vec2(pPrevChar->m_X, pPrevChar->m_Y));
					p = Position - Dir * g_pData->m_Weapons.m_aId[Weapon].m_Muzzleoffsetx;
					Graphics()->QuadsSetRotation(angle(Dir));
					RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[Weapon].m_aSpriteMuzzles[s_LastIteX], 0);
					RenderTools()->DrawSprite(p.x, p.y, 160.0f);
				}
			}
			break;

		case WEAPON_HAMMER:
		{
			// Static position for hammer
			p = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
			p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;
			// if attack is under way, bash stuffs
			if (Direction.x < 0)
			{
				Graphics()->QuadsSetRotation(-pi / 2 - State.GetAttach()->m_Angle * pi * 2);
				p.x -= g_pData->m_Weapons.m_aId[Weapon].m_Offsetx;
			}
			else
			{
				Graphics()->QuadsSetRotation(-pi / 2 + State.GetAttach()->m_Angle * pi * 2);
				p.x += g_pData->m_Weapons.m_aId[Weapon].m_Offsetx;
			}
			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize);
		}
		break;

		case WEAPON_BLOCK:
		{
			const float RecoilTick = (Client()->GameTick() - Player.m_AttackTick + s_LastIntraTick) / 5.0f;
			const float Recoil = RecoilTick < 1.0f ? sinf(RecoilTick * pi) : 0.0f;
			p = Position + Direction * -(g_pData->m_Weapons.m_aId[Weapon].m_Offsetx - Recoil * 40.0f);
			p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;

			if (Direction.x < 0)
				p.x += g_pData->m_Weapons.m_aId[Weapon].m_Offsetx;
			else
				p.x -= g_pData->m_Weapons.m_aId[Weapon].m_Offsetx;

			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize);
		}
		break;

		case WEAPON_SWORD:
		{
			const float RecoilTick = (Client()->GameTick() - Player.m_AttackTick + s_LastIntraTick) / 5.0f;
			const float Recoil = RecoilTick < 1.0f ? sinf(RecoilTick * pi) : 0.0f;
			p = Position + Direction * -(g_pData->m_Weapons.m_aId[Weapon].m_Offsetx - Recoil * 40.0f);
			p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;

			if (Direction.x < 0)
				p.x += 10.4f;
			else
				p.x -= 10.4f;

			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize);

			if (Client()->GameTick() - Player.m_AttackTick <= (SERVER_TICK_SPEED / 6))
			{
				Graphics()->QuadsSetRotation(angle(Direction));
				RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[Weapon].m_aSpriteMuzzles[random_int() % g_pData->m_Weapons.m_aId[Weapon].m_NumSpriteMuzzles], 0);
				RenderTools()->DrawSprite(p.x, p.y, 102.0f);
				Graphics()->QuadsEnd();
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_SPARK].m_Id);
				Graphics()->QuadsBegin();
				RenderTools()->SelectSprite((rand() % (SPRITE_SPARK2_3 - SPRITE_SPARK1_1)) + SPRITE_SPARK1_1, 0);
				RenderTools()->DrawSprite(p.x, p.y, 102.0f);
				Graphics()->QuadsEnd();
				Graphics()->TextureSet(g_pData->m_aImages[IMAGE_SWORD].m_Id);
				Graphics()->QuadsBegin();
			}
		}
		break;

		case WEAPON_SPARK:
		{
			const float RecoilTick = (Client()->GameTick() - Player.m_AttackTick + s_LastIntraTick) / 5.0f;
			const float Recoil = RecoilTick < 1.0f ? sinf(RecoilTick * pi) : 0.0f;
			p = Position + Direction * -(g_pData->m_Weapons.m_aId[Weapon].m_Offsetx - Recoil * 40.0f);
			p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;

			if (Direction.x < 0)
				p.x += 10.4f;
			else
				p.x -= 10.4f;

			Graphics()->QuadsSetRotation((Client()->GameTick() - Player.m_AttackTick) * 1.f);
			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize);
			RenderTools()->SelectSprite((rand() % (SPRITE_SPARK2_3 - SPRITE_SPARK1_1)) + SPRITE_SPARK1_1, 0);
			RenderTools()->DrawSprite(p.x, p.y, 60.0f);
		}
		break;

		case WEAPON_SCYTHE:
		{
			if (m_pClient->m_aClients[ClientID].m_Predicted.m_Input.m_Fire & 1)
			{
				const float RecoilTick = (350 + s_LastIntraTick) / 5.0f;
				const float Recoil = RecoilTick < 1.0f ? sinf(RecoilTick * pi) : 0.0f;
				p = Position + Direction * (g_pData->m_Weapons.m_aId[Weapon].m_Offsetx - Recoil * 100.0f);
				p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;

				RenderTools()->SelectSprite(SPRITE_WEAPON_SCYTHE_MUZZLE4, 0);
			}
			else
			{
				p = Position + vec2(State.GetAttach()->m_X, State.GetAttach()->m_Y);
				p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;
			}
			// if attack is under way, bash stuffs
			if (Direction.x < 0)
				p.x -= 4.4f;
			else
				p.x += 4.4f;

			Graphics()->QuadsSetRotation(Player.m_MeleeSpinTick / 100.f);
			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize + 24.f);
		}
		break;

		default:
		{
			// TODO: should be an animation
			const float RecoilTick = (Client()->GameTick() - Player.m_AttackTick + s_LastIntraTick) / 5.0f;
			const float Recoil = RecoilTick < 1.0f ? sinf(RecoilTick * pi) : 0.0f;
			p = Position + Direction * (g_pData->m_Weapons.m_aId[Weapon].m_Offsetx - Recoil * 10.0f);
			p.y += g_pData->m_Weapons.m_aId[Weapon].m_Offsety;
			if ((Client()->GameTick() - Player.m_AttackTick) < g_pData->m_Weapons.m_aId[Weapon].m_Firedelay / 50)
				Graphics()->QuadsSetRotation((Client()->GameTick() % 360 * 8) / 8);
			RenderTools()->DrawSprite(p.x, p.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize);

			if (Player.m_Weapon == WEAPON_GUN || Player.m_Weapon == WEAPON_SHOTGUN)
			{
				// check if we're firing stuff
				if (g_pData->m_Weapons.m_aId[Weapon].m_NumSpriteMuzzles)
				{
					const float MuzzleTick = Client()->GameTick() - Player.m_AttackTick + s_LastIntraTick;
					const int IteX = random_int() % g_pData->m_Weapons.m_aId[Weapon].m_NumSpriteMuzzles;
					static int s_LastIteX = IteX;
					if (UpdateSingleAnimationFrame)
						s_LastIteX = IteX;

					if (MuzzleTick < g_pData->m_Weapons.m_aId[Weapon].m_Muzzleduration && g_pData->m_Weapons.m_aId[Weapon].m_aSpriteMuzzles[s_LastIteX])
					{
						const bool FlipY = Direction.x < 0.0f;
						const float OffsetY = g_pData->m_Weapons.m_aId[Weapon].m_Muzzleoffsety * (FlipY ? 1 : -1);
						const vec2 MuzzlePos = p + Direction * g_pData->m_Weapons.m_aId[Weapon].m_Muzzleoffsetx + vec2(-Direction.y, Direction.x) * OffsetY;
						RenderTools()->SelectSprite(g_pData->m_Weapons.m_aId[Weapon].m_aSpriteMuzzles[s_LastIteX], FlipY ? SPRITE_FLAG_FLIP_Y : 0);
						RenderTools()->DrawSprite(MuzzlePos.x, MuzzlePos.y, g_pData->m_Weapons.m_aId[Weapon].m_VisualSize);
					}
				}
			}
		}
		break;
		}
		Graphics()->QuadsEnd();

		switch (Player.m_Weapon)
		{
		case WEAPON_SWORD:
		case WEAPON_GUN:
		case WEAPON_SPARK:
		case WEAPON_BLOCK:
			RenderTools()->RenderTeeHand(&RenderInfo, p, Direction, -3 * pi / 4, vec2(-15, 4));
			break;
		case WEAPON_SHOTGUN:
			RenderTools()->RenderTeeHand(&RenderInfo, p, Direction, -pi / 2, vec2(-5, 4));
			break;
		case WEAPON_GRENADE:
		case WEAPON_SCYTHE:
			RenderTools()->RenderTeeHand(&RenderInfo, p, Direction, -pi / 2, vec2(-4, 7));
			break;
		}
	}

	// render the "shadow" tee
	if (m_pClient->m_LocalClientID == ClientID && Config()->m_Debug)
	{
		vec2 GhostPosition = mix(vec2(pPrevChar->m_X, pPrevChar->m_Y), vec2(pPlayerChar->m_X, pPlayerChar->m_Y), Client()->IntraGameTick());
		CTeeRenderInfo Ghost = RenderInfo;
		for (int p = 0; p < NUM_SKINPARTS; p++)
			Ghost.m_aColors[p].a *= 0.5f;
		RenderTools()->RenderTee(&State, &Ghost, Player.m_Emote, Direction, GhostPosition); // render ghost
	}

	RenderTools()->RenderTee(&State, &RenderInfo, Player.m_Emote, Direction, Position);

	if (pPlayerInfo->m_PlayerFlags & PLAYERFLAG_CHATTING)
	{
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);
		Graphics()->QuadsBegin();
		RenderTools()->SelectSprite(SPRITE_DOTDOT);
		IGraphics::CQuadItem QuadItem(Position.x + 24, Position.y - 40, 64, 64);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}

	{
		const int Max = g_pData->m_Mana.m_Max;
		float Progress = clamp(Player.m_Mana, 0, Max) / (float)Max;
		Graphics()->TextureSet(g_pData->m_aImages[IMAGE_GAME].m_Id);
		Graphics()->QuadsBegin();
		float x = Position.x - 32.f;
		float y = Position.y - 40.f;
		Progress = clamp(Progress, 0.0f, 1.0f);
		const float EndWidth = 6.0f;
		const float BarHeight = 18.0f;
		const float WholeBarWidth = 64.f;
		const float MiddleBarWidth = WholeBarWidth - (EndWidth * 2.0f);

		IGraphics::CQuadItem QuadStartFull(x, y, EndWidth, BarHeight);
		RenderTools()->SelectSprite(&g_pData->m_aSprites[SPRITE_NINJA_BAR_FULL_LEFT]);
		Graphics()->SetColor(255.f, 32.f, 96.f, 255.f);
		Graphics()->QuadsDrawTL(&QuadStartFull, 1);
		x += EndWidth;

		const float FullBarWidth = MiddleBarWidth * Progress;
		const float EmptyBarWidth = MiddleBarWidth - FullBarWidth;

		// full bar
		IGraphics::CQuadItem QuadFull(x, y, FullBarWidth, BarHeight);

		CDataSprite SpriteBarFull = g_pData->m_aSprites[SPRITE_NINJA_BAR_FULL];
		// prevent pixel puree, select only a small slice
		if (Progress < 0.1f)
		{
			int spx = SpriteBarFull.m_X;
			int spy = SpriteBarFull.m_Y;
			float w = SpriteBarFull.m_W * 0.1f; // magic here
			int h = SpriteBarFull.m_H;
			int cx = SpriteBarFull.m_pSet->m_Gridx;
			int cy = SpriteBarFull.m_pSet->m_Gridy;
			float x1 = spx / (float)cx;
			float x2 = (spx + w - 1 / 32.0f) / (float)cx;
			float y1 = spy / (float)cy;
			float y2 = (spy + h - 1 / 32.0f) / (float)cy;

			Graphics()->QuadsSetSubset(x1, y1, x2, y2);
		}
		else
			RenderTools()->SelectSprite(&SpriteBarFull);

		Graphics()->SetColor(255.f, 32.f, 96.f, 255.f);

		Graphics()->QuadsDrawTL(&QuadFull, 1);

		// empty bar
		// select the middle portion of the sprite so we don't get edge bleeding
		const CDataSprite SpriteBarEmpty = g_pData->m_aSprites[SPRITE_NINJA_BAR_FULL];
		{
			float spx = SpriteBarEmpty.m_X + 0.1f;
			float spy = SpriteBarEmpty.m_Y;
			float w = SpriteBarEmpty.m_W * 0.5f;
			int h = SpriteBarEmpty.m_H;
			int cx = SpriteBarEmpty.m_pSet->m_Gridx;
			int cy = SpriteBarEmpty.m_pSet->m_Gridy;
			float x1 = spx / (float)cx;
			float x2 = (spx + w - 1 / 32.0f) / (float)cx;
			float y1 = spy / (float)cy;
			float y2 = (spy + h - 1 / 32.0f) / (float)cy;

			Graphics()->QuadsSetSubset(x1, y1, x2, y2);
		}
		Graphics()->SetColor(1.f, 0.f, 1.f, 255.f);
		IGraphics::CQuadItem QuadEmpty(x + FullBarWidth, y, EmptyBarWidth, BarHeight);
		Graphics()->QuadsDrawTL(&QuadEmpty, 1);

		x += MiddleBarWidth;

		IGraphics::CQuadItem QuadEndEmpty(x, y, EndWidth, BarHeight);
		RenderTools()->SelectSprite(&g_pData->m_aSprites[SPRITE_NINJA_BAR_EMPTY_RIGHT]);
		Graphics()->SetColor(1.f, 0.f, 1.f, 255.f);
		Graphics()->QuadsDrawTL(&QuadEndEmpty, 1);

		Graphics()->QuadsEnd();
	}
	CGameClient::CClientData *pClientData = &m_pClient->m_aClients[ClientID];
	if (pClientData->m_EmoticonStart != -1 && pClientData->m_Emoticon >= 0 && pClientData->m_Emoticon < NUM_EMOTICONS)
	{
		// adjust start tick if world paused; not if demo paused because ticks are synchronized with demo
		static int s_LastGameTick = Client()->GameTick();
		if (m_pClient->IsWorldPaused())
			pClientData->m_EmoticonStart += Client()->GameTick() - s_LastGameTick;
		s_LastGameTick = Client()->GameTick();

		const float TotalEmoteLifespan = 2 * Client()->GameTickSpeed();
		const float SinceStart = (Client()->GameTick() - pClientData->m_EmoticonStart) / (float)Client()->GameTickSpeed();
		const float FromEnd = (pClientData->m_EmoticonStart + TotalEmoteLifespan - Client()->GameTick()) / (float)Client()->GameTickSpeed();
		if (SinceStart > 0.0f && FromEnd > 0.0f)
		{
			const float Size = 64.0f;
			const float Alpha = FromEnd < 0.2f ? FromEnd / 0.2f : 1.0f;
			const float HeightFactor = SinceStart < 0.1f ? SinceStart / 0.1f : 1.0f;
			const float Wiggle = SinceStart < 0.2f ? SinceStart / 0.2f : 0.0f;

			Graphics()->TextureSet(g_pData->m_aImages[IMAGE_EMOTICONS].m_Id);
			Graphics()->QuadsBegin();
			Graphics()->QuadsSetRotation(pi / 6 * sinf(5 * Wiggle));
			Graphics()->SetColor(Alpha, Alpha, Alpha, Alpha);
			RenderTools()->SelectSprite(SPRITE_OOP + pClientData->m_Emoticon); // pClientData->m_Emoticon is an offset from the first emoticon
			IGraphics::CQuadItem QuadItem(Position.x, Position.y - 23 - Size * HeightFactor / 2.0f, Size, Size * HeightFactor);
			Graphics()->QuadsDraw(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}
	}
}

void CPlayers::OnRender()
{
	if (Client()->State() < IClient::STATE_ONLINE)
		return;

	static const CNetObj_PlayerInfo *s_apInfo[MAX_CLIENTS];
	static CTeeRenderInfo s_aRenderInfo[MAX_CLIENTS];

	// update RenderInfo for ninja
	bool IsTeamplay = (m_pClient->m_GameInfo.m_GameFlags & GAMEFLAG_TEAMS) != 0;
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		if (!m_pClient->m_Snap.m_aCharacters[i].m_Active)
			continue;

		s_apInfo[i] = (const CNetObj_PlayerInfo *)Client()->SnapFindItem(IClient::SNAP_CURRENT, NETOBJTYPE_PLAYERINFO, i);
		s_aRenderInfo[i] = m_pClient->m_aClients[i].m_RenderInfo;

		if (m_pClient->m_Snap.m_aCharacters[i].m_Cur.m_Weapon == WEAPON_NINJA)
		{
			// change the skin for the player to the ninja
			int Skin = m_pClient->m_pSkins->Find("x_ninja", true);
			if (Skin != -1)
			{
				const CSkins::CSkin *pNinja = m_pClient->m_pSkins->Get(Skin);
				for (int p = 0; p < NUM_SKINPARTS; p++)
				{
					if (IsTeamplay)
					{
						s_aRenderInfo[i].m_aTextures[p] = pNinja->m_apParts[p]->m_ColorTexture;
						int ColorVal = m_pClient->m_pSkins->GetTeamColor(true, pNinja->m_aPartColors[p], m_pClient->m_aClients[i].m_Team, p);
						s_aRenderInfo[i].m_aColors[p] = m_pClient->m_pSkins->GetColorV4(ColorVal, p == SKINPART_MARKING);
					}
					else if (pNinja->m_aUseCustomColors[p])
					{
						s_aRenderInfo[i].m_aTextures[p] = pNinja->m_apParts[p]->m_ColorTexture;
						s_aRenderInfo[i].m_aColors[p] = m_pClient->m_pSkins->GetColorV4(pNinja->m_aPartColors[p], p == SKINPART_MARKING);
					}
					else
					{
						s_aRenderInfo[i].m_aTextures[p] = pNinja->m_apParts[p]->m_OrgTexture;
						s_aRenderInfo[i].m_aColors[p] = vec4(1.0f, 1.0f, 1.0f, 1.0f);
					}
				}
			}
		}
	}

	// render other players in two passes, first pass we render the other, second pass we render our self
	for (int p = 0; p < 4; p++)
	{
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			// only render active characters
			if (!m_pClient->m_Snap.m_aCharacters[i].m_Active || !s_apInfo[i])
				continue;

			//
			bool Local = m_pClient->m_LocalClientID == i;
			if ((p % 2) == 0 && Local)
				continue;
			if ((p % 2) == 1 && !Local)
				continue;

			CNetObj_Character *pPrevChar = &m_pClient->m_Snap.m_aCharacters[i].m_Prev;
			CNetObj_Character *pCurChar = &m_pClient->m_Snap.m_aCharacters[i].m_Cur;

			if (p < 2)
			{
				RenderHook(
					pPrevChar,
					pCurChar,
					&s_aRenderInfo[i],
					i);
			}
			else
			{
				RenderPlayer(
					pPrevChar,
					pCurChar,
					s_apInfo[i],
					&s_aRenderInfo[i],
					i);
			}
		}
	}
}
