/*
 *  TaskManager.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 09.02.09.
 *  code under LGPL
 *
 */

#include "ThreadPool.h"
#include "TaskManager.h"
#include "Debug.h"
#include "ReadWriteLock.h"
#include "Command.h"

TaskManager* taskManager = NULL;

void InitTaskManager() {
	if(!taskManager) {
		taskManager = new TaskManager();
	}
}

void UnInitTaskManager() {
	if(taskManager) {
		delete taskManager;
		taskManager = NULL;
	}
}


TaskManager::TaskManager() {
	quitSignal = false;
	mutex = SDL_CreateMutex();
	taskFinished = SDL_CreateCond();
	queueThreadWakeup = SDL_CreateCond();
		
	struct QueuedTaskHandler : Action {
		TaskManager* manager;
		QueuedTaskHandler(TaskManager* m) : manager(m) {}
		int handle() {
			ScopedLock lock(manager->mutex);
			while(true) {
				if(manager->queuedTasks.size() > 0) {
					Action* act = manager->queuedTasks.front();
					manager->queuedTasks.pop_front();
					SDL_mutexV(manager->mutex);
					act->handle();
					delete act;
					SDL_mutexP(manager->mutex);
					continue;
				}
				if(manager->quitSignal) {
					return 0;
				}
				SDL_CondWait(manager->queueThreadWakeup, manager->mutex);
			}
		}
	};
	queueThread = threadPool->start(new QueuedTaskHandler(this), "queued task handler");
}

TaskManager::~TaskManager() {
	SDL_mutexP(mutex);
	while(runningTasks.size() > 0) {
		notes << "waiting for " << runningTasks.size() << " task(s) to finish:" << endl;
		for(std::set<Task*>::iterator i = runningTasks.begin(); i != runningTasks.end(); ++i) {
			notes << "  " << (*i)->name << endl;
		}
		SDL_CondWait(taskFinished, mutex);
	}
	SDL_mutexV(mutex);
	
	finishQueuedTasks();
	
	SDL_DestroyMutex(mutex);
	SDL_DestroyCond(taskFinished);
	SDL_DestroyCond(queueThreadWakeup);
}


void TaskManager::start(Task* t, bool queued) {
	Mutex::ScopedLock tlock(t->mutex);

	{
		ScopedLock lock(mutex);
		runningTasks.insert(t);		
	}
	
	struct TaskHandler : Action {
		Task* task;
		int handle() {
			{
				Mutex::ScopedLock lock(task->mutex);
				task->state = Task::TS_QUEUED ? Task::TS_RUNNINGQUEUED : Task::TS_RUNNING;
			}
			int ret = task->handle();
			if(task->manager == NULL) {
				errors << "TaskManager::TaskHandler: invalid task with unset manager" << endl;
				return ret;
			}
			SDL_mutexP(task->manager->mutex);
			task->manager->runningTasks.erase(task);
			SDL_CondSignal(task->manager->taskFinished);
			SDL_mutexV(task->manager->mutex);
			delete task;
			return ret;
		}
	};
	TaskHandler* handler = new TaskHandler();
	handler->task = t;
	if(t->manager != NULL)
		errors << "Task->manager must be NULL" << endl;
	t->manager = this;
	
	if(!queued) {
		t->state = Task::TS_WAITFORIMMSTART;
		threadPool->start(handler, t->name + " handler", true);
	} else {
		ScopedLock lock(mutex);
		if(quitSignal) {
			warnings << "tried to start queued task " << t->name << " when queued task manager was already shutdown" << endl;
			return;
		}
		t->state = Task::TS_QUEUED;
		queuedTasks.push_back(handler);
		SDL_CondSignal(queueThreadWakeup);
	}
}

void TaskManager::finishQueuedTasks() {
	{
		ScopedLock lock(mutex);
		if(quitSignal) // means that we have already finished
			return;
		quitSignal = true;
		SDL_CondSignal(queueThreadWakeup);
	}
	
	threadPool->wait(queueThread);
	queueThread = NULL;
}


void TaskManager::dumpState(CmdLineIntf& cli) const {
	ScopedLock lock(mutex);
	for(std::set<Task*>::const_iterator i = runningTasks.begin(); i != runningTasks.end(); ++i) {
		Task::State state;
		{
			Mutex::ScopedLock lock((*i)->mutex);
			state = (*i)->state;
		}
		switch(state) {
			case Task::TS_QUEUED: cli.writeMsg("task '" + (*i)->name + "': queued for execution"); break;
			case Task::TS_WAITFORIMMSTART: cli.writeMsg("task '" + (*i)->name + "': wait for immediate start"); break;
			case Task::TS_RUNNINGQUEUED: cli.writeMsg("task '" + (*i)->name + "': running from queue"); break;
			case Task::TS_RUNNING: cli.writeMsg("task '" + (*i)->name + "': running independently"); break;
			case Task::TS_INVALID: cli.writeMsg("task '" + (*i)->name + "': invalid state"); break;
		}
	}
	if(runningTasks.size() == 0)
		cli.writeMsg("no tasks queued/running");
}

