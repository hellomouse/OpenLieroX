/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Profile system
// Created 13/8/02
// Jason Boettcher


#ifndef __PROFILESYSTEM_H__
#define __PROFILESYSTEM_H__

#include <SDL.h>
#include <string>
#include <stdlib.h>
#include "SmartPointer.h"
#include "CGameSkin.h"

#define		PROFILE_VERSION		5

/*
#define		PRF_HUMAN			0
#define		PRF_COMPUTER		1
*/

class CWorm;
class CWormInputHandler;

struct WormType {
	virtual ~WormType() {}
	virtual CWormInputHandler* createInputHandler(CWorm* w) = 0;
	virtual int toInt() = 0;
	static WormType* fromInt(int type);
	bool operator==(const WormType& t) { return &t == this; }
	bool operator!=(const WormType& t) { return &t != this; }
};

extern WormType* PRF_HUMAN;
extern WormType* PRF_COMPUTER;



// AI Difficulty
#define		AI_EASY			0
#define		AI_MEDIUM		1
#define		AI_HARD			2
#define		AI_XTREME		3



// Player profile structure
class profile_t { public:
	profile_t() : iID(0), iType(PRF_HUMAN->toInt()), nDifficulty(AI_EASY), R(0), G(0), B(0), iTeam(0), tNext(NULL) {}

	int				iID;

	int		iType;
	std::string		sName;
    int             nDifficulty;
	std::string		sUsername;
	std::string		sPassword;
	Uint8			R,G,B;
	std::string		sWeaponSlots[5];
	int				iTeam;
	CWormSkin		cSkin;

	profile_t *tNext;
};



int		LoadProfiles();
void	SaveProfiles();
void	ShutdownProfiles();

void    AddDefaultPlayers();
void	SaveProfile(FILE *fp, profile_t *p);
void    AddProfile(const std::string& name, const std::string& skin, const std::string& username, const std::string& password,  int R, int G, int B, int type, int difficulty);
void	LoadProfile(FILE *fp, int id);
int		FindProfileID();
void	DeleteProfile(int id);

profile_t *GetProfiles();
profile_t *FindProfile(int id);
profile_t *FindProfile(const std::string& name);

std::string FindFirstCPUProfileName();


#endif  //  __PROFILESYSTEM_H__
