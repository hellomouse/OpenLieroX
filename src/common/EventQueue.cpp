/////////////////////////////////////////
//
//   OpenLieroX
//
//   event queue
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////


#include <list>
#include <cassert>
#include "ThreadPool.h"
#include <SDL_events.h>
#include <time.h>

#include "LieroX.h"
#include "EventQueue.h"
#include "ReadWriteLock.h"
#include "Debug.h"
#include "InputEvents.h"

static void InitQuitSignalHandler();

EventQueue* mainQueue = NULL;

struct EventQueueIntern {
	SDL_mutex* mutex;
	SDL_cond* cond;
	std::list<EventItem> queue;
	EventQueueIntern() : mutex(NULL), cond(NULL) {}
	void init() {
		mutex = SDL_CreateMutex();
		cond = SDL_CreateCond();
	}
	void uninit() { // WARNING: don't call this if any other thread could be using this queue
		while(true) {
			// We swapped them because some of the code we are calling here at the cleanup could again access us
			// and that would either cause deadlocks or crashes, thus we still need a vaild eventqueue at this point.
			std::list<EventItem> tmpList;
			{
				ScopedLock lock(mutex);
				tmpList.swap(queue);
			}
			if(tmpList.size() > 0)
				warnings << "there are still " << tmpList.size() << " pending events in the event queue" << endl;
			else
				// finally new events anymore -> quit
				break;
			
			for(std::list<EventItem>::iterator i = tmpList.begin(); i != tmpList.end(); ++i) {
				/* We execute all custom events because we want to ensure that
				 * each thrown event is also execute.
				 * We do some important cleanup and we stop other threads there which
				 * would otherwise never stop, for example the Timer system. */
				if(i->type == SDL_USEREVENT && i->user.code == UE_CustomEventHandler) {
					((Action*)i->user.data1)->handle();
					delete (Action*)i->user.data1;
				}
			}
		}
		
		SDL_DestroyMutex(mutex);
		mutex = NULL;
		
		SDL_DestroyCond(cond);
		cond = NULL;
	}
};

EventQueue::EventQueue() {
	data = new EventQueueIntern();
	data->init();
}

EventQueue::~EventQueue() {
	assert(data != NULL);
	data->uninit();
	delete data; data = NULL;
}


void InitEventQueue() {
	if(!mainQueue) mainQueue = new EventQueue();
	InitQuitSignalHandler();
}

void ShutdownEventQueue() {
	if(mainQueue) {
		delete mainQueue;
		mainQueue = NULL;
	}
}

bool EventQueue::poll(EventItem& event) {
	ScopedLock lock(data->mutex);
	
	if( data->queue.empty() )
		return false;

	event = data->queue.front();	
	data->queue.pop_front();
	
	return true;
}

bool EventQueue::wait(EventItem& event) {
	ScopedLock lock(data->mutex);
	
	while( data->queue.empty() ) {
		SDL_CondWait( data->cond, data->mutex );
	}
	
	event = data->queue.front();
	data->queue.pop_front();
	
	return true;
}

bool EventQueue::push(const EventItem& event) {
	ScopedLock lock(data->mutex);
	
	// TODO: some limit if queue too full? it could happen that OLX eats up all mem if there is no limit
	data->queue.push_back(event);
	
	SDL_CondSignal(data->cond);
	
	return true;
}

static EventItem CustomEvent(Action* act) {
	// TODO: this is a bit hacky because we still use the SDL_Event structure
	SDL_Event ev;
	ev.type = SDL_USEREVENT;
	ev.user.code = UE_CustomEventHandler;
	ev.user.data1 = act;
	ev.user.data2 = NULL;
	return ev;
}

bool EventQueue::push(Action* act) {
	return push(CustomEvent(act));
}

void EventQueue::copyCustomEvents(const _Event* oldOwner, _Event* newOwner) {
	ScopedLock lock(data->mutex);

	for(std::list<EventItem>::iterator i = data->queue.begin(); i != data->queue.end(); ++i) {
		if(i->type == SDL_USEREVENT && i->user.code == UE_CustomEventHandler) {
			CustomEventHandler* hndl = dynamic_cast<CustomEventHandler*>( (Action*)i->user.data1 );
			if(hndl && hndl->owner() == oldOwner) {
				data->queue.insert(i, CustomEvent(hndl->copy(newOwner)));
			}
		}
	}
}

void EventQueue::removeCustomEvents(const _Event* owner) {
	ScopedLock lock(data->mutex);
	
	for(std::list<EventItem>::iterator i = data->queue.begin(); i != data->queue.end(); ) {
		std::list<EventItem>::iterator last = i; ++i;
		const SDL_Event& ev = *last;
		if(ev.type == SDL_USEREVENT && ev.user.code == UE_CustomEventHandler) {
			CustomEventHandler* hndl = dynamic_cast<CustomEventHandler*>( (Action*)ev.user.data1 );
			if(hndl && hndl->owner() == owner) {
				delete (Action*)ev.user.data1;
				data->queue.erase(last);
			}
		}
	}
}




#if defined(WIN32)

#include <windows.h>

 
static BOOL QuitSignalHandler( DWORD fdwCtrlType ) 
{ 
	EventItem ev;
	ev.type = SDL_QUIT;
	mainQueue->push(ev);
	tLX->bQuitCtrlC = true; // Set the special CTRL-C flag, so Dedicated Server won't try to close the non-existant pipe
	tLX->bQuitGame = true;
	return TRUE;
}


static void InitQuitSignalHandler()
{
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) QuitSignalHandler, TRUE );
	SetConsoleMode(stdin, ENABLE_PROCESSED_INPUT);
}

#else // MacOSX, Linux, Unix

#include <signal.h>

static void QuitSignalHandler(int sig)
{
	if(mainQueue) {
		SDL_Event event;
		event.type = SDL_QUIT;
		mainQueue->push(event);
	} else {
		warnings << "got quit-signal and mainQueue is not set" << endl;
	}
	if(tLX) tLX->bQuitGame = true;
}

static void InitQuitSignalHandler()
{
	signal(SIGINT, QuitSignalHandler);
	signal(SIGTERM, QuitSignalHandler);
}

#endif

