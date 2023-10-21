/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <game/mapitems.h>

#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>
#include "mod.h"

CGameControllerMOD::CGameControllerMOD(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	// game
	m_apFlags[0] = 0;
	m_apFlags[1] = 0;
	m_pGameType = "FW";
	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS;

	// context
	m_Context = pGameServer;
}

// balancing
bool CGameControllerMOD::CanBeMovedOnBalance(int ClientID) const
{
	return true;
}

// event
int CGameControllerMOD::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int WeaponID)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, WeaponID);
	int HadFlag = 1;

	// drop flags
	// do nothing lmao

	return HadFlag;
}

void CGameControllerMOD::OnFlagReturn(CFlag *pFlag)
{
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
	GameServer()->SendGameMsg(GAMEMSG_CTF_RETURN, -1);
}

bool CGameControllerMOD::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	int Team = -1;
	if(Index == ENTITY_FLAGSTAND_RED) Team = TEAM_RED;
	if(Index == ENTITY_FLAGSTAND_BLUE) Team = TEAM_BLUE;
	if(Team == -1 || m_apFlags[Team])
		return false;

	CFlag *F = new CFlag(&GameServer()->m_World, Team, Pos);
	m_apFlags[Team] = F;
	return true;
}

// game
bool CGameControllerMOD::DoWincheckMatch()
{
	// check score win condition
	if(getTeamSize(TEAM_BLUE) == 0 && m_apFlags[TEAM_BLUE]->FWHidden())
	{
		// GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "debug", "red win");
		m_aTeamscore[TEAM_RED] = 999;
		EndMatch();
		return true;
	}
	if(getTeamSize(TEAM_RED) == 0 && m_apFlags[TEAM_RED]->FWHidden())
	{
		// GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "debug", "blue win");
		m_aTeamscore[TEAM_BLUE] = 999;
		EndMatch();
		return true;
	}
	return false;
}

// general
void CGameControllerMOD::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	CNetObj_GameDataFlag *pGameDataFlag = static_cast<CNetObj_GameDataFlag *>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATAFLAG, 0, sizeof(CNetObj_GameDataFlag)));
	if(!pGameDataFlag)
		return;

	pGameDataFlag->m_FlagDropTickRed = 0;
	if(m_apFlags[TEAM_RED])
	{
		if(m_apFlags[TEAM_RED]->IsAtStand())
			pGameDataFlag->m_FlagCarrierRed = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
			pGameDataFlag->m_FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			pGameDataFlag->m_FlagCarrierRed = FLAG_TAKEN;
			pGameDataFlag->m_FlagDropTickRed = m_apFlags[TEAM_RED]->GetDropTick();
		}
	}
	else
		pGameDataFlag->m_FlagCarrierRed = FLAG_MISSING;
	pGameDataFlag->m_FlagDropTickBlue = 0;
	if(m_apFlags[TEAM_BLUE])
	{
		if(m_apFlags[TEAM_BLUE]->IsAtStand())
			pGameDataFlag->m_FlagCarrierBlue = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
			pGameDataFlag->m_FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCID();
		else
		{
			pGameDataFlag->m_FlagCarrierBlue = FLAG_TAKEN;
			pGameDataFlag->m_FlagDropTickBlue = m_apFlags[TEAM_BLUE]->GetDropTick();
		}
	}
	else
		pGameDataFlag->m_FlagCarrierBlue = FLAG_MISSING;
}

void CGameControllerMOD::Tick()
{
	m_Context->set_global_flags(m_apFlags);
	m_Context->set_GameController(this);

	IGameController::Tick();

	for(int i = 0; i < GetPlayersCount(); i++) {
		char* FlagsMsg = (char*)malloc(sizeof(char) * 64);
		str_format(FlagsMsg, 64, "Red flag HP: %d | Blue flag HP: %d", maximum(m_apFlags[TEAM_RED]->m_Health, 0), maximum(m_apFlags[TEAM_BLUE]->m_Health, 0));
		GameServer()->SendBroadcast(FlagsMsg, ((CPlayer**)GetPlayers())[i]->GetCID());
		free(FlagsMsg);
	}

	if(GameServer()->m_World.m_ResetRequested || GameServer()->m_World.m_Paused)
		return;
	
	for(int i = 0; i < m_pForcedSpectatorsIndex; i++)
		for(int j = 0; j < GetPlayersCount(); j++) {
			CPlayer* p = ((CPlayer**)GetPlayers())[j];
			if(m_pForcedSpectators[i] == p->GetCID())
				if(p->GetTeam() != TEAM_SPECTATORS)
					DoTeamChange(p, TEAM_SPECTATORS, false);
		}

	for(int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];

		if(!F)
			continue;

		//
		if(F->GetCarrier())
		{
			if(m_apFlags[fi^1] && m_apFlags[fi^1]->IsAtStand())
			{
				if(distance(F->GetPos(), m_apFlags[fi^1]->GetPos()) < CFlag::ms_PhysSize + CCharacter::ms_PhysSize)
				{
					// CAPTURE! \o/ // unused
					// m_aTeamscore[fi^1] += 100;
					// F->GetCarrier()->GetPlayer()->m_Score += 5;
					// float Diff = Server()->Tick() - F->GetGrabTick();

					// char aBuf[64];
					// str_format(aBuf, sizeof(aBuf), "flag_capture player='%d:%s' team=%d time=%.2f",
					// 	F->GetCarrier()->GetPlayer()->GetCID(),
					// 	Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()),
					// 	F->GetCarrier()->GetPlayer()->GetTeam(),
					// 	Diff / (float)Server()->TickSpeed()
					// );
					// GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

					// GameServer()->SendGameMsg(GAMEMSG_CTF_CAPTURE, fi, F->GetCarrier()->GetPlayer()->GetCID(), Diff, -1);
					// for(int i = 0; i < 2; i++)
					// 	m_apFlags[i]->Reset();
					// // do a win check(capture could trigger win condition)
					// if(DoWincheckMatch())
					// 	return;
				}
			}
		}
		else
		{
			CCharacter *apCloseCCharacters[MAX_CLIENTS];
			int Num = GameServer()->m_World.FindEntities(F->GetPos(), CFlag::ms_PhysSize, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
			for(int i = 0; i < Num; i++)
			{
				if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->GetPos(), apCloseCCharacters[i]->GetPos(), NULL, NULL))
					continue;

				if(apCloseCCharacters[i]->GetPlayer()->GetTeam() == F->GetTeam())
				{
					// return the flag // unused
					// if(!F->IsAtStand())
					// {
					// 	CCharacter *pChr = apCloseCCharacters[i];
					// 	pChr->GetPlayer()->m_Score += 1;

					// 	char aBuf[256];
					// 	str_format(aBuf, sizeof(aBuf), "flag_return player='%d:%s' team=%d",
					// 		pChr->GetPlayer()->GetCID(),
					// 		Server()->ClientName(pChr->GetPlayer()->GetCID()),
					// 		pChr->GetPlayer()->GetTeam()
					// 	);
					// 	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					// 	GameServer()->SendGameMsg(GAMEMSG_CTF_RETURN, -1);
					// 	F->Reset();
					// }
				}
				else
				{
					// F->FWHide();

					// char aBuf[256];
					// str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s' team=%d",
					// 	F->GetCarrier()->GetPlayer()->GetCID(),
					// 	Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()),
					// 	F->GetCarrier()->GetPlayer()->GetTeam()
					// );
					// GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					// GameServer()->SendGameMsg(GAMEMSG_CTF_GRAB, fi, -1);
					break;
				}
			}
		}
	}
	// do a win check(grabbing flags could trigger win condition)
	DoWincheckMatch();
}

void CGameControllerMOD::OnReset() {
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "debug", "resetting game", true);

	IGameController::OnReset();

	ClearForcedSpectators();

	for(int fi = 0; fi < 2; fi++)
	{
		CFlag *F = m_apFlags[fi];
		F->FWUnhide();
		F->Reset();
	}

	for(int i = 0; i < GetPlayersCount(); i++)
		DoTeamChange(((CPlayer**)GetPlayers())[i], TEAM_RED, false);

	m_Context->m_GameController->ForceTeamBalance();
}