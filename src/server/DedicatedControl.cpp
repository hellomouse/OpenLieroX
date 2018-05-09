/*
 *  DedicatedControl.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 11.01.08.
 *  code under LGPL
 *
 */

// define HAVE_BOOST if you want to compile dedicated server for Win32 and have Boost headers installed.

// A workaround for a bug in boost - doesn't compile with DEBUG_NEW in MSVC 2005
#if defined(_MSC_VER) && _MSC_VER == 1400
#undef new
#endif

#include <string>
#include <sstream>
#include <stdexcept>
#include "ThreadPool.h"
#include <fcntl.h>
#include <iostream>

#include "Debug.h"
#include "LieroX.h"
#include "IpToCountryDB.h"
#include "DedicatedControl.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "CMap.h"
#include "ProfileSystem.h"
#include "CClient.h"
#include "CServer.h"
#include "CWorm.h"
#include "CGameScript.h"
#include "Unicode.h"
#include "Protocol.h"
#include "CScriptableVars.h"
#include "CClientNetEngine.h"
#include "CChannel.h"
#include "CServerConnection.h"
#include "CServerNetEngine.h"
#include "Command.h"
#include "Process.h"
#include "Event.h"
#include "CGameMode.h"
#include "Cache.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Menu.h"

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif


using std::endl;



static DedicatedControl* dedicatedControlInstance = NULL;

DedicatedControl* DedicatedControl::Get() 
{
	return dedicatedControlInstance;
}

bool DedicatedControl::Init() {
	dedicatedControlInstance = new DedicatedControl();
	return dedicatedControlInstance->Init_priv();
}

void DedicatedControl::Uninit() {
	delete dedicatedControlInstance;
	dedicatedControlInstance = NULL;
}



struct ScriptCmdLineIntf : CmdLineIntf {
	Process pipe;
	ThreadPoolItem* thread;
	

	ScriptCmdLineIntf() : thread(NULL) {
	}
	
	~ScriptCmdLineIntf() {
		breakCurrentScript();
	}

	
	void pushReturnArg(const std::string& str) {
		pipeOut() << ":" << str << endl;
	}
	
	void finalizeReturn() {
		pipeOut() << "." << endl;
	}

	void writeMsg(const std::string& str, CmdLineMsgType type) {
		// TODO: handle type
		hints << "Script Dedicated: " << str << endl;
	}

	
	std::ostream& pipeOut() {
		return pipe.in();
	}
	
	void closePipe() {
		pipe.close();
	}
	
	bool havePipe() {
		return thread != NULL;
	}

	
	
	bool breakCurrentScript() {
		if(thread) {
			notes << "waiting for pipeThread ..." << endl;
			pipe.close();
			threadPool->wait(thread, NULL);
		}
		thread = NULL;
		
		return true;
	}
	
	bool loadScript(const std::string& script, const std::string& scriptArgs)
	{
		breakCurrentScript();
		return loadScript_Pipe(script, scriptArgs);
	}

	
	bool loadScript_Pipe(const std::string& script, const std::string& scriptArgs) {
		breakCurrentScript();
		
		std::string scriptfn = GetAbsolutePath(GetFullFileName(script));
		if(script != "/dev/null") {
			if(!IsFileAvailable(scriptfn, true)) {
				errors << "Dedicated: " << scriptfn << " not found" << endl;
				return false;
			}			

			notes << "Dedicated server: running script \"" << scriptfn << "\" args \"" << scriptArgs << "\"" << endl;
			// HINT: If a script need this change in his own directory, it is a bug in the script.
			// If we change into script directory, ded function "getfullfilename" won't work anymore
			std::vector<std::string> args = explode(script + " " + scriptArgs, " ");
			if(!pipe.open(scriptfn, args, ExtractDirectory(scriptfn) )) {
				errors << "cannot start dedicated server - cannot run script " << scriptfn << endl;
				return false;
			}

			thread = threadPool->start(&ScriptCmdLineIntf::pipeThreadFunc, this, "Ded pipe watcher");
		}
		else
			notes << "Dedicated server: not running any script" << endl;
		
		return true;
	}

	void ScriptSignalHandler() {} // stub
	
	
	
	// Pipe functions
	
	// reading lines from pipe-out and put them to pipeOutput
	static int pipeThreadFunc(void* o) {
		ScriptCmdLineIntf* owner = (ScriptCmdLineIntf*)o;

		while(!owner->pipe.out().eof()) {
			std::string buf;
			getline(owner->pipe.out(), buf);
			
			Execute( owner, buf );
		}
		return 0;
	}
	
};

struct StdinCmdLineIntf : CmdLineIntf {
	ThreadPoolItem* thread;
	
	StdinCmdLineIntf() {
		thread = threadPool->start(&StdinCmdLineIntf::stdinThreadFunc, this, "Ded stdin watcher");
	}
	
	~StdinCmdLineIntf() {
		notes << "waiting for stdinThread ..." << endl;
		threadPool->wait(thread, NULL);
		thread = NULL;
	}
	
	void pushReturnArg(const std::string& str) {
		notes << "Dedicated return: " << str << endl;
	}
	
	void finalizeReturn() {
		notes << "Dedicated return." << endl;
	}

	void writeMsg(const std::string& str, CmdLineMsgType type) {
		// TODO: handle type
		hints << "STDIN Dedicated: " << str << endl;
	}
	
	
	// reading lines from stdin and put them to pipeOutput
	static int stdinThreadFunc(void* o) {
		StdinCmdLineIntf* owner = (StdinCmdLineIntf*)o;

#ifndef WIN32
		// TODO: there's no fcntl for Windows!
		if(fcntl(0, F_SETFL, O_NONBLOCK) == -1)
#endif
			warnings << "ERROR setting standard input into non-blocking mode" << endl;

		while(true) {
			std::string buf;
			while(true) {
				SDL_Delay(10); // TODO: select() here
				if(tLX->bQuitGame) return 0;

				char c;

				if(read(0, &c, 1) >= 0) {
					if(c == '\n') break;
					// TODO: why is this needed? is that WIN32 only?
					if(c == -52) return 0;  // CTRL-C
					buf += c;
				}
			}

			Execute( owner, buf );
		}
		return 0;
	}	
};




struct DedIntern {

	static DedIntern* Get() { return (DedIntern*)dedicatedControlInstance->internData; }

	ScriptCmdLineIntf* scriptInterface;
	StdinCmdLineIntf* stdinInterface;
	
	bool quitSignal;
	SDL_mutex* pendingSignalsMutex;
	bool waitingForNextSignal;
	struct Signal {
		std::string name;
		std::list<std::string> args;
		Signal(const std::string& n, const std::list<std::string>& a) : name(n), args(a) {}
	};
	std::list<Signal> pendingSignals;
	
	

	DedIntern() :
		scriptInterface(NULL), stdinInterface(NULL),
		quitSignal(false),
		pendingSignalsMutex(NULL), waitingForNextSignal(false)
	{
		dedicatedControlInstance->internData = this;
		pendingSignalsMutex = SDL_CreateMutex();

		scriptInterface = new ScriptCmdLineIntf();
		stdinInterface = new StdinCmdLineIntf();
	}
	
	~DedIntern() {
		Sig_Quit();
		quitSignal = true;

		delete stdinInterface; stdinInterface = NULL;
		delete scriptInterface; scriptInterface = NULL;
		
		SDL_DestroyMutex(pendingSignalsMutex);

		notes << "DedicatedControl destroyed" << endl;
		dedicatedControlInstance->internData = NULL;
	}

	
	void pushSignal(const std::string& name, const std::list<std::string>& args = std::list<std::string>()) {
		if(!scriptInterface->havePipe()) return;
		ScopedLock lock(pendingSignalsMutex);
		if(waitingForNextSignal) {
			if(pendingSignals.size() > 0)
				errors << "Dedicated pending signals queue should be empty when the script is waiting for next signal" << endl;
			scriptInterface->pushReturnArg(name);
			for(std::list<std::string>::const_iterator i = args.begin(); i != args.end(); ++i)
				scriptInterface->pushReturnArg(*i);
			scriptInterface->finalizeReturn();
			waitingForNextSignal = false;
		}
		else {
			pendingSignals.push_back(Signal(name, args));
			if(pendingSignals.size() > 1000) {
				warnings << "Dedicated pending signals queue got too full!" << endl;
				pendingSignals.pop_front();
			}
		}
	}
	
	void pushSignal(const std::string& name, const std::string& arg1) {
		std::list<std::string> args; args.push_back(arg1);
		pushSignal(name, args);
	}
	
	void pushSignal(const std::string& name, const std::string& arg1, const std::string& arg2) {
		std::list<std::string> args; args.push_back(arg1); args.push_back(arg2);
		pushSignal(name, args);
	}
	
	void pushSignal(const std::string& name, const std::string& arg1, const std::string& arg2, const std::string& arg3) {
		std::list<std::string> args; args.push_back(arg1); args.push_back(arg2); args.push_back(arg3);
		pushSignal(name, args);
	}
	

	// ----------------------------------
	// ----------- signals --------------

	void Sig_LobbyStarted() { pushSignal("lobbystarted"); }
	void Sig_GameLoopStart() { pushSignal("gameloopstart"); }
	void Sig_GameLoopEnd() {
		pushSignal("gameloopend");
		if(currentGameState() != S_SVRLOBBY && currentGameState() != S_CLILOBBY)
			// This is because of the current game logic: It will end the game
			// loop and then return to the lobby but only in the case if we got a
			// BackToLobby-signal before; if we didn't get such a signal and
			// the gameloop was ended, that means that the game was stopped
			// completely.
			notes << "gameloopend without backtolobby -> stop" << endl;
	}
	void Sig_WeaponSelections() { pushSignal("weaponselections"); }
	void Sig_GameStarted() { pushSignal("gamestarted"); }
	void Sig_BackToLobby() { pushSignal("backtolobby"); }
	void Sig_Quit() { pushSignal("quit"); scriptInterface->closePipe(); }

	void Sig_Connecting(const std::string& addr) { pushSignal("connecting", addr); }
	void Sig_ConnectError(const std::string& err) { pushSignal("connecterror", err); }
	void Sig_Connected() { pushSignal("connected"); }
	void Sig_ClientError() { pushSignal("clienterror"); }
	void Sig_ClientConnectionError(const std::string& err) { pushSignal("connectionerror", err); }
	void Sig_ClientGameStarted() { pushSignal("clientgamestarted"); }
	void Sig_ClientGotoLobby() { pushSignal("clientbacktolobby"); }

	void Sig_NewWorm(CWorm* w) { pushSignal("newworm", itoa(w->getID()), w->getName()); }
	void Sig_WormLeft(CWorm* w) { pushSignal("wormleft", itoa(w->getID()), w->getName()); }
	void Sig_ChatMessage(CWorm* w, const std::string& message) { pushSignal("chatmessage", itoa(w->getID()), message); }
	void Sig_PrivateMessage(CWorm* w, CWorm* to, const std::string& message) { pushSignal("privatemessage", itoa(w->getID()), itoa(to->getID()), message); }
	void Sig_WormDied(int died, int killer) { pushSignal("wormdied", itoa(died), itoa(killer)); }
	void Sig_WormSpawned(CWorm* worm) { pushSignal("wormspawned", itoa(worm->getID())); }
	void Sig_WormGotAdmin(CWorm* w) { pushSignal("wormgotadmin", itoa(w->getID())); }
	void Sig_WormAuthorized(CWorm* w) { pushSignal("wormauthorized", itoa(w->getID())); }

	void Sig_Timer(const std::string& name) { pushSignal("timer", name); }


	// ----------------------------------
	// ---------- frame handlers --------

	void Frame_ServerLobby() {
		// Process the server & client frames
		cServer->Frame();
		cClient->Frame();
	}

	void Frame_Playing() {
		// we don't have to process server/client frames here as it is done already by the main loop
	}

	void Frame_ClientConnecting() {
		cClient->Frame();

		// are we connected?
		if(cClient->getStatus() == NET_CONNECTED) {
			Sig_Connected();
			return;
		}

		// error?
		if(cClient->getBadConnection()) {
			warnings << "Bad connection: " << cClient->getBadConnectionMsg() << endl;
			Sig_ConnectError(cClient->getBadConnectionMsg());
			cClient->Shutdown();
			return;
		}
	}

	void Frame_ClientLobby() {
		// Process the client
		cClient->Frame();

		// If there is a client error, leave
		if(cClient->getClientError()) {
			Sig_ClientError();
			return;
		}

		// If we have started, leave the frontend
		if(cClient->getGameReady()) {
			// Leave the frontend
			*DeprecatedGUI::bGame = true;
			DeprecatedGUI::tMenu->bMenuRunning = false;
			tLX->iGameType = GME_JOIN;
			Sig_ClientGameStarted();
			return;
		}


		// Check if the communication link between us & server is still ok
		if(cClient->getServerError()) {
			warnings << "Client connection error: " << cClient->getServerErrorMsg() << endl;
			Sig_ClientConnectionError(cClient->getServerErrorMsg());
			cClient->Disconnect();
			cClient->Shutdown();
			SetQuitEngineFlag("Frame_ClientLobby: connection error");
			return;
		}
	}

	void Frame_Basic() {

		// TODO: make this clean!
		// TODO: no static var here
		// TODO: not a single timer, the script should register as much as it want
		// TODO: each timer should be configurable
		static AbsTime lastTimeHandlerCalled = tLX->currentTime;
		if( tLX->currentTime > lastTimeHandlerCalled + 1.0f ) // Call once per second, when no signals pending, TODO: configurable from ded script
		{
			Sig_Timer("second-ticker"); // TODO ...
			lastTimeHandlerCalled = tLX->currentTime;
		}


		// Some debugging stuff
		// TODO: This really much spams my logs. There should be a better way to inform about this.
#if 0
//#if DEBUG
		int fps = GetFPS();
		static AbsTime lastFpsPrint = tLX->currentTime;
		if (tLX->currentTime - lastFpsPrint >= 20.0f)  {
			notes << "Current FPS: " << fps << endl;
			lastFpsPrint = tLX->currentTime;
		}

		static AbsTime lastBandwidthPrint = tLX->currentTime;
		if (tLX->currentTime - lastBandwidthPrint >= 20.0f)  {
			// Upload and download rates
			float up = 0;
			float down = 0;
			
			// Get the rates
			if( tLX->iGameType == GME_JOIN ) {
				if(cClient->getChannel()) {
					down = cClient->getChannel()->getIncomingRate() / 1024.0f;
					up = cClient->getChannel()->getOutgoingRate() / 1024.0f;
				}
			}
			else if( tLX->iGameType == GME_HOST ) {
				down = cServer->GetDownload() / 1024.0f;
				up = cServer->GetUpload() / 1024.0f;
			}

			notes << "Current upload rate: " << up << " kB/s" << endl;
			notes << "Current download rate: " << down << " kB/s" << endl;
			lastBandwidthPrint = tLX->currentTime;
		}
#endif


		switch(currentGameState()) {
			case S_INACTIVE: break;
			case S_SVRLOBBY: Frame_ServerLobby(); break;
			case S_SVRWEAPONS: Frame_Playing(); break;
			case S_SVRPLAYING: Frame_Playing(); break;
			case S_CLICONNECTING: Frame_ClientConnecting(); break;
			case S_CLILOBBY: Frame_ClientLobby(); break;
			case S_CLIPLAYING: Frame_Playing(); break;
			case S_CLIWEAPONS: Frame_Playing(); break;
		}
	}
};


DedicatedControl::DedicatedControl() : internData(NULL) {}
DedicatedControl::~DedicatedControl() { if(internData) delete internData; internData = NULL; }

// gets called from static DedicatedControl::Init()
bool DedicatedControl::Init_priv() {
	DedIntern* dedIntern = new DedIntern; // constr will assign this->internData to this obj
	if(tLXOptions->sDedicatedScript != "" && tLXOptions->sDedicatedScript != "/dev/null") {
		if(IsAbsolutePath(tLXOptions->sDedicatedScript)) {
			dedIntern->scriptInterface->loadScript(tLXOptions->sDedicatedScript, tLXOptions->sDedicatedScriptArgs);
			return true;
		}

		if(strStartsWith(tLXOptions->sDedicatedScript, "scripts/")) { // old clients will use it like that
			return dedIntern->scriptInterface->loadScript(tLXOptions->sDedicatedScript, tLXOptions->sDedicatedScriptArgs);
			return true;
		}
		
		dedIntern->scriptInterface->loadScript("scripts/" + tLXOptions->sDedicatedScript, tLXOptions->sDedicatedScriptArgs);
	}
	else
		dedIntern->scriptInterface->loadScript("/dev/null", "");
	
	return true;
}


// This is the main game loop, the one that do all the simulation etc.
void DedicatedControl::GameLoopStart_Signal() { internData->Sig_GameLoopStart(); }
void DedicatedControl::GameLoopEnd_Signal() { internData->Sig_GameLoopEnd(); }
void DedicatedControl::LobbyStarted_Signal() { internData->Sig_LobbyStarted(); }
void DedicatedControl::BackToServerLobby_Signal() { internData->Sig_BackToLobby(); }
void DedicatedControl::BackToClientLobby_Signal() { internData->Sig_ClientGotoLobby(); }
void DedicatedControl::WeaponSelections_Signal() { internData->Sig_WeaponSelections(); }
void DedicatedControl::GameStarted_Signal() { internData->Sig_GameStarted(); }
void DedicatedControl::Connecting_Signal(const std::string& addr) { internData->Sig_Connecting(addr); }
void DedicatedControl::NewWorm_Signal(CWorm* w) { internData->Sig_NewWorm(w); }
void DedicatedControl::WormLeft_Signal(CWorm* w) { internData->Sig_WormLeft(w); }
void DedicatedControl::ChatMessage_Signal(CWorm* w, const std::string& message) { internData->Sig_ChatMessage(w,message); }
void DedicatedControl::PrivateMessage_Signal(CWorm* w, CWorm* to, const std::string& message) { internData->Sig_PrivateMessage(w,to,message); }
void DedicatedControl::WormDied_Signal(CWorm* worm, CWorm* killer) { internData->Sig_WormDied(worm->getID(), killer ? killer->getID() : -1); }
void DedicatedControl::WormSpawned_Signal(CWorm* worm){ internData->Sig_WormSpawned(worm); };
void DedicatedControl::WormGotAdmin_Signal(CWorm* worm){ internData->Sig_WormGotAdmin(worm); };
void DedicatedControl::WormAuthorized_Signal(CWorm* worm){ internData->Sig_WormAuthorized(worm); };
void DedicatedControl::Custom_Signal(const std::list<std::string>& args) { internData->pushSignal("custom", args); }

void DedicatedControl::Menu_Frame() { internData->Frame_Basic(); }
void DedicatedControl::GameLoop_Frame() { internData->Frame_Basic(); }

void DedicatedControl::ChangeScript(const std::string& filename, const std::string& args) {
	{
		ScopedLock lock(internData->pendingSignalsMutex);
		internData->waitingForNextSignal = false;
		internData->pendingSignals.clear();
	}

	if(filename == "" || filename == "/dev/null")
		internData->scriptInterface->loadScript("/dev/null", "");
	else
		internData->scriptInterface->loadScript("scripts/" + filename, args);
}

bool DedicatedControl::GetNextSignal(CmdLineIntf* sender) {
	if(sender != internData->scriptInterface) {
		sender->writeMsg("nextsignal can only be called from the dedicated script");
		return true; // true means that finalizeReturn() should be called from caller of this func
	}
	
	ScopedLock lock(internData->pendingSignalsMutex);
	if(internData->pendingSignals.size() > 0) {
		DedIntern::Signal sig = internData->pendingSignals.front();
		internData->pendingSignals.pop_front();
		sender->pushReturnArg(sig.name);
		for(std::list<std::string>::iterator i = sig.args.begin(); i != sig.args.end(); ++i)
			sender->pushReturnArg(*i);
		return true;
	}
	else {
		// do nothing, we will send the next signal when it arrives
		internData->waitingForNextSignal = true;
		return false;
	}
}
