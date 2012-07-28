#pragma once
#include "Base.h"
#include <BWAPI.h>
#include "BuildManager.h"
#include "BuildQueue.h"
#include <cmath>
#include "LoadBuildOrder.h"
#include <queue>
#include <time.h>
#include "WorldManager.h"

class UnitManager
{
public:
	UnitManager(void);
	~UnitManager(void);
	void evalute();
private:
	void manageStartOfGame();
	void manageResourceGathering();
	void manageSupply();
	void manageBuildOrder();

	BWAPI::UnitType getNextRequiredUnit(BWAPI::UnitType unitType);
	int inPipelineCount(BWAPI::UnitType unitType);
	int inPipelineCount(BWAPI::UnitType unitType, bool includeIncomplete);

	//Keeps track of which building we are waiting to finish building
	BWAPI::UnitType buildOrderBuilding;

	BuildQueue* buildQueue;
	BuildOrder* buildOrder;
};