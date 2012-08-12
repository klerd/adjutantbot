#include "MilitaryManager.h"
#include "AdjutantAIModule.h"

MilitaryManager::MilitaryManager(void)
{
}

void MilitaryManager::evalute()
{
	Utils::log("Entering MilitaryManager", 1);

	if (WorldManager::Instance().isTerrainAnalyzed && BWAPI::Broodwar->getFrameCount() % 50 == 0)
	{
		std::vector<UnitGroup*>* myArmyGroups = WorldManager::Instance().myArmyGroups;

		//For now, just have the whole army in group(1)
		if (myArmyGroups->size() < 2)
		{
			myArmyGroups->push_back(new UnitGroup());
		}

		UnitGroup* baseGroup = myArmyGroups->front();

		//Recruit worker if needed
		manageFightingWorkers();

		//Init baseGroup location
		if (baseGroup->targetPosition == BWAPI::Positions::None)
		{
			baseGroup->targetPosition = WorldManager::Instance().myHomeBase->baseLocation->getRegion()->getCenter();
		}

		if (baseGroup->size() > 0)
		{
			for each (BWAPI::Unit* unit in (*baseGroup->unitVector))
			{
				myArmyGroups->at(1)->addUnit(unit);
				unit->attack(myArmyGroups->at(1)->targetPosition);
			}

			baseGroup->removeAllUnits();
		}
		
		//Determine army behavior
		BWAPI::Position armyPosition = BWAPI::Positions::None;

		//Threats
		if (WorldManager::Instance().enemy->getUnits().size() != 0)
		{
			static BWAPI::Position oldPosition = BWAPI::Position(0,0);

			//Mob Attack threat location
			if (BWAPI::Broodwar->getFrameCount() % 50 == 0)
			{
				armyPosition = (*WorldManager::Instance().enemy->getUnits().begin())->getPosition();
				oldPosition = (*WorldManager::Instance().enemy->getUnits().begin())->getPosition();
			}
			else
			{
				armyPosition = oldPosition;
			}
		}
		else if (WorldManager::Instance().enemyHomeRegion != NULL 
			&& (WorldManager::Instance().getMyArmyValue() - WorldManager::Instance().getEnemyArmyValue() > 1000))
		{
			static bool searchAndDestroy = false;
			static BWAPI::Position oldPosition = BWAPI::Position(0,0);

			if (myArmyGroups->at(1)->getCentroid().getDistance(WorldManager::Instance().enemyHomeRegion->getCenter()) < 400
				|| (BWAPI::Broodwar->getFrameCount() > 25000 && WorldManager::Instance().getMyArmyValue() > (WorldManager::Instance().getEnemyArmyValue() * 10))
				|| BWAPI::Broodwar->getFrameCount() > 50000)
			{
				searchAndDestroy = true;
			}

			if (searchAndDestroy)
			{
				if (BWAPI::Broodwar->getFrameCount() % 500 == 0)
				{
					if (WorldManager::Instance().enemyHistoricalUnitMap.size() > 0)
					{
						std::pair<int, HistoricalUnitInfo> firstPair = (*WorldManager::Instance().enemyHistoricalUnitMap.begin());
						armyPosition = BWAPI::Position(firstPair.second.getPosition());
					}
					else
					{
						int randTileX = rand() % BWAPI::Broodwar->mapWidth();
						int randTileY = rand() % BWAPI::Broodwar->mapHeight();

						armyPosition = BWAPI::Position(BWAPI::TilePosition(randTileX, randTileY));
					}
					oldPosition = armyPosition;
				}
				else
				{
					armyPosition = oldPosition;
				}
			}
			else
			{
				armyPosition = WorldManager::Instance().enemyHomeRegion->getCenter();
			}
		}
		else
		{
			//TODO:account for multiple chokepoints
			BWTA::Region* homeRegion = WorldManager::Instance().myHomeBase->baseLocation->getRegion();
			
			if (homeRegion->getChokepoints().size() > 0)
			{
				//armyPosition = (*WorldManager::Instance().myHomeRegion->getChokepoints().begin())->getCenter();
				armyPosition = homeRegion->getCenter();
			}
			else
			{
				armyPosition = homeRegion->getCenter();
			}
		}

		myArmyGroups->at(1)->targetPosition = armyPosition;

		//Attack to location for all not near it
		for each (BWAPI::Unit* unit in (*myArmyGroups->at(1)->unitVector))
		{
			bool useLowLevelControl = false;
			BWAPI::Unit* closestEnemy = NULL;

			if (WorldManager::Instance().enemy->getUnits().size() != 0)
			{
				closestEnemy = Utils::getClosestUnit(unit, &WorldManager::Instance().enemy->getUnits());
				if (closestEnemy->getDistance(unit) < 500)
				{
					useLowLevelControl = true;
				}	
			}

			if (useLowLevelControl)
			{
				if (! unit->isAttacking())
				{
					if (closestEnemy->getDistance(unit) < unit->getType().groundWeapon().maxRange())
					{
						unit->attack(closestEnemy);
					}
					else
					{
						unit->attack(closestEnemy->getPosition());					
					}
				}
			}
			else if (unit->getDistance(myArmyGroups->at(1)->getCentroid()) > 500)
			{
				unit->attack(myArmyGroups->at(1)->getCentroid());
			}
			else if (unit->getDistance(myArmyGroups->at(1)->targetPosition) > 300)
			{
				unit->attack(myArmyGroups->at(1)->targetPosition);
			}
		}

		//Control comsat use
		std::vector<BWAPI::Unit*> comsatVector = WorldManager::Instance().myUnitMap[BWAPI::UnitTypes::Terran_Comsat_Station];
		std::vector<BWAPI::Unit*> sweepVector = WorldManager::Instance().myUnitMap[BWAPI::UnitTypes::Spell_Scanner_Sweep];

		if (! comsatVector.empty() && (sweepVector.empty() || sweepVector.size() == 0))
		{
			for each (BWAPI::Unit* enemyUnit in WorldManager::Instance().enemy->getUnits())
			{
				if (enemyUnit->isCloaked() || enemyUnit->isBurrowed() || enemyUnit->getType().hasPermanentCloak())
				{
					for each (BWAPI::Unit* comsat in comsatVector)
					{
						if (comsat->getEnergy() > BWAPI::TechTypes::Scanner_Sweep.energyUsed())
						{
							comsat->useTech(BWAPI::TechTypes::Scanner_Sweep, enemyUnit->getPosition());
							break;
						}
					}
				}
			}
		}
	}

	Utils::log("Leaving MilitaryManager", 1);
}

void MilitaryManager::manageFightingWorkers()
{
	int totalProtectedThreat = 0;
	Threat* protectionThreat = NULL;

	for each (Threat* threat in WorldManager::Instance().threatVector)
	{
		if (threat->isInProtectedRegion())
		{
			protectionThreat = threat;
			totalProtectedThreat += threat->getAttackValue();
		}
	}

	while (totalProtectedThreat > WorldManager::getMyArmyValue() 
		&& WorldManager::Instance().myHomeBase->getTotalWorkerCount() > 0)
	{
		BWAPI::Unit* armyWorker = WorldManager::Instance().myHomeBase->
			removeWorkerNear(protectionThreat->getCentroid());
		
		if (armyWorker != NULL)
		{
			WorldManager::Instance().myArmyGroups->front()->addUnit(armyWorker);
		}
		else
		{
			break;
		}
	}

	if (totalProtectedThreat == 0)
	{
		for each (UnitGroup* group in  (*WorldManager::Instance().myArmyGroups))
		{
			group->removeType(BWAPI::UnitTypes::Terran_SCV);
		}
	}
}

void MilitaryManager::manageDefense()
{

}

MilitaryManager::~MilitaryManager(void)
{
}