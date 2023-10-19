/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_MOD_H
#define GAME_SERVER_GAMEMODES_MOD_H
#include <game/server/gamecontroller.h>
#include <game/server/entity.h>

class CGameControllerMOD : public IGameController
{
	// balancing
	virtual bool CanBeMovedOnBalance(int ClientID) const;

	// game
	class CFlag *m_apFlags[2];

	// context
	class CGameContext *m_Context;

	virtual bool DoWincheckMatch();

public:
	CGameControllerMOD(class CGameContext *pGameServer);
	
	// event
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual void OnFlagReturn(class CFlag *pFlag);
	virtual bool OnEntity(int Index, vec2 Pos);

	void OnReset() override;

	// general
	virtual void Snap(int SnappingClient);
	virtual void Tick();
};

#endif

