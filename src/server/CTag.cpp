/*
 OpenLieroX
 
 tag gamemode
 
 created 2009-02-14
 code under LGPL
 */

#include <iostream>
#include "CWorm.h"
#include "Options.h"
#include "Consts.h"
#include "CServer.h"
#include "CClient.h"
#include "CGameMode.h"
#include "Consts.h"

class CTag : public CGameMode {
protected:
	virtual void TagWorm(CWorm *worm);
	virtual void TagRandomWorm();
public:
	
	virtual void PrepareGame();
	virtual void PrepareWorm(CWorm* worm);
	virtual bool Spawn(CWorm* worm, CVec pos);
	virtual void Kill(CWorm* victim, CWorm* killer);
	virtual void Drop(CWorm* worm);
	virtual void Simulate();
	virtual bool CheckGameOver();
	virtual int  GeneralGameType();
	virtual int CompareWormsScore(CWorm *w1, CWorm *w2);
	virtual int  Winner();
	virtual std::string Name() { return "Tag"; }
	virtual GameInfoGroup getGameInfoGroupInOptions() { return GIG_Tag; }
	
protected:
	bool bFirstBlood;
	int  iKillsInRow[MAX_WORMS];
	int  iDeathsInRow[MAX_WORMS];
};


void CTag::PrepareGame()
{
	bFirstBlood = true;
	for(int i = 0; i < MAX_WORMS; i++) {
		iKillsInRow[i] = 0;
		iDeathsInRow[i] = 0;
	}
}

void CTag::PrepareWorm(CWorm* worm)
{
	worm->setTagIT(false);
	worm->setTagTime(TimeDiff(0));
}

bool CTag::Spawn(CWorm* worm, CVec pos)
{
	worm->Spawn(pos);
	return true;
}

void CTag::Kill(CWorm* victim, CWorm* killer)
{
	CGameMode::Kill(victim, killer);

	// Tag another worm
	if (victim->getTagIT())  {
		if (killer && killer != victim)  {
			if (killer->getLives() == WRM_OUT)  // The killer got killed and out in the meanwhile
				TagRandomWorm();
			else
				TagWorm(killer);
		} else // Suicide or no killer
			TagRandomWorm();
	}
}

///////////////////
// Returns true if w1 has a better score than w2
int CTag::CompareWormsScore(CWorm *w1, CWorm *w2)
{
	// Tag time
	if (w1->getTagTime() > w2->getTagTime()) return 1;
	if (w1->getTagTime() < w2->getTagTime()) return -1;

	return CGameMode::CompareWormsScore(w1, w2);
}

void CTag::Drop(CWorm* worm)
{
	CGameMode::Drop(worm);
	// Tag another worm
	if (worm->getTagIT())
		TagRandomWorm();
}

void CTag::Simulate()
{
	// Increase the tag time
	bool have_it = false;
	for (int i = 0; i < MAX_WORMS; i++)  {
		if (cServer->getWorms()[i].isUsed() && cServer->getWorms()[i].getTagIT() && cServer->getWorms()[i].getLives() != WRM_OUT)  {
			if (cServer->getWorms()[i].getAlive())
				cServer->getWorms()[i].incrementTagTime(tLX->fRealDeltaTime);
			have_it = true;
			break;
		}
	}

	// No worm is tagged (happens at the beginning of the game)
	if (!have_it)
		TagRandomWorm();

	// Check if any of the worms reached the maximum tag time
	if (tLXOptions->tGameInfo.iTagLimit > 0)
		for (int i = 0; i < MAX_WORMS; i++)
			if(cServer->getWorms()[i].isUsed() && cServer->getWorms()[i].getTagTime() >= tLXOptions->tGameInfo.iTagLimit * 60.0f)
				cServer->RecheckGame();
}

bool CTag::CheckGameOver()
{
	if(CGameMode::CheckGameOver()) return true;
	

	if (tLXOptions->tGameInfo.iTagLimit > 0)  {
		int wormid = HighestScoredWorm();

		// Check if any of the worms reached the maximum tag time
		if(wormid >= 0 && cServer->getWorms()[wormid].getTagTime() >= tLXOptions->tGameInfo.iTagLimit * 60.0f) {
			// TODO: make configureable
			cServer->SendGlobalText(cServer->getWorms()[wormid].getName() + " has reached the maximum tag time", TXT_NORMAL);
			notes << cServer->getWorms()[wormid].getName() << " has reached the maximum tag time" << endl;
			return true;
		}
	}

	return false;
}

int CTag::GeneralGameType()
{
	return GMT_TIME;
}

int CTag::Winner()
{
	return HighestScoredWorm();
}

void CTag::TagWorm(CWorm *worm)
{
	// Safety check
	if(!worm || cServer->getState() != SVS_PLAYING)
		return;

	// Go through all the worms, setting their tag to false
	for(short i = 0; i < MAX_WORMS; i++)
		if(cServer->getWorms()[i].isUsed())
			cServer->getWorms()[i].setTagIT(false);

	// Some safety warnings
	if (!worm->isUsed())  {
		errors << "Tagging an unused worm (ID " << worm->getID() << ")" << endl;
	}


	worm->setTagIT(true);

	// Let everyone know this guy is tagged
	cServer->SendWormTagged(worm);

	// Take care of the <none> tag
	if (networkTexts->sWormIsIt != "<none>")  {
		cServer->SendGlobalText((replacemax(networkTexts->sWormIsIt, "<player>", worm->getName(), 1)),
						TXT_NORMAL);
	}
}

////////////////
// Tag a random worm
void CTag::TagRandomWorm()
{
	TimeDiff time = AbsTime::Max() - AbsTime();
	std::vector<int> all_lowest;

	// Go through finding the worm with the lowest tag time
	// A bit more fairer then random picking
	unsigned short i;
	for(i=0;i<MAX_WORMS;i++) {
		if(cServer->getWorms()[i].isUsed() && cServer->getWorms()[i].getLives() != WRM_OUT) {
			if(cServer->getWorms()[i].getTagTime() < time) {
				time = cServer->getWorms()[i].getTagTime();
			}
		}
	}

	// Find all the worms that have the lowest time
	for (i=0;i<MAX_WORMS;i++)  {
		if (cServer->getWorms()[i].isUsed() && cServer->getWorms()[i].getLives() != WRM_OUT)  {
			if (cServer->getWorms()[i].getTagTime() == time)  {
				all_lowest.push_back(i);
			}
		}
	}

	// Choose a random worm from all those having the lowest time
	int random_lowest = GetRandomInt((int)all_lowest.size()-1);


	// Tag the lowest tagged worm
	TagWorm(&cServer->getWorms()[all_lowest[random_lowest]]);
}

static CTag gameMode;
CGameMode* gameMode_Tag = &gameMode;
