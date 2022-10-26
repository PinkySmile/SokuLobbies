//
// Created by PinkySmile on 26/10/2022.
//

#ifndef SOKULOBBIES_GUARDEDMUTEX_HPP
#define SOKULOBBIES_GUARDEDMUTEX_HPP


#include <mutex>

class GuardedMutex {
private:
	std::mutex &_mutex;
	bool _locked = false;

public:
	GuardedMutex(std::mutex &m);
	~GuardedMutex();
	void lock();
	void unlock();
};


#endif //SOKULOBBIES_GUARDEDMUTEX_HPP
