#pragma once
#include <BWAPI.h>
#include <queue>
#include "Utils.h"
#include "WorldManager.h"

class MilitaryManager
{
public:
	MilitaryManager(void);
	~MilitaryManager(void);
	void evalute();
	void manageDefense();
};