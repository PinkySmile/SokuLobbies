//
// Created by PinkySmile on 26/10/2022.
//

#include "GuardedMutex.hpp"

GuardedMutex::GuardedMutex(std::mutex &m) :
	_mutex(m)
{
}

GuardedMutex::~GuardedMutex()
{
	this->unlock();
}

void GuardedMutex::lock() {
	if (!this->_locked)
		this->_mutex.lock();
	this->_locked = true;
}

void GuardedMutex::unlock() {
	if (!this->_locked)
		return;
	this->_mutex.unlock();
	this->_locked = false;
}