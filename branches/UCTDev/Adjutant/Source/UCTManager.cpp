#include "UCTManager.h"

UCTManager::UCTManager(void)
{
	this->rootRoundNode = NULL;
	this->roundInProgress = false;
}

void UCTManager::evaluate()
{
	int myUnitCount = WorldManager::Instance().myArmyVector->size();
	int enemyUnitCount = WorldManager::Instance().enemyHistoricalUnitMap.size();

	//Check for round beginning and end
	if (this->roundInProgress)
	{
		//Round just ended
		if (myUnitCount == 0 || enemyUnitCount == 0)
		{
			this->onRoundEnd();
			this->roundInProgress = false;
		}
	}
	else
	{
		//Round just started
		if (myUnitCount > 0 && enemyUnitCount > 0)
		{
			this->onRoundStart();
			this->roundInProgress = true;
		}
	}

	//Check for decision point (1 or more idle groups)
	unsigned int busyGroupCount = WorldManager::Instance().groupAttackMap.size();

	for each (std::vector<UnitGroup*>* groupVector in WorldManager::Instance().groupJoinVector)
	{
		busyGroupCount += groupVector->size();
	}

	if (this->roundInProgress && 
		myUnitCount > 0 &&
		busyGroupCount < WorldManager::Instance().myArmyGroups->size())
	{
		this->onDecisionPoint(NULL);
	}
}

void UCTManager::onRoundStart()
{
	BWAPI::Broodwar->printf("Entering - OnRoundStart()");

	//Form groups
	this->formMyGroups();

	//Determine first starting node and initial team sizes
	this->maxFriendlyGroups = WorldManager::Instance().myArmyGroups->size();
	this->maxEnemyGroups = WorldManager::Instance().threatVector.size();
	this->rootRoundNode = new UCTNode(this->maxFriendlyGroups, this->maxEnemyGroups);

	//Manually trigger a decision point with the root node
	this->onDecisionPoint(this->rootRoundNode);
}

void UCTManager::onRoundEnd()
{
	//Temp
	for each (UCTAction* action in this->rootRoundNode->possibleActions)
	{
		if (action->type == UCTAction::AttackAction)
		{
			UCTAttackAction* attackAction = (UCTAttackAction*)action;

			double averageReward = 0;

			if (attackAction->visitCount > 0)
			{
				averageReward = attackAction->totalReward / attackAction->visitCount;
			}

			BWAPI::Broodwar->printf("Attack %d->%d [v:%d][avg_r:%f]", 
				attackAction->myGroupIndex, 
				attackAction->enemyGroupIndex,
				attackAction->visitCount,
				averageReward);
		}
	}

	//Clean up data structures
	WorldManager::Instance().groupAttackMap.clear();
	WorldManager::Instance().groupJoinVector.clear();
	this->rootRoundNode = NULL;

	for each (UCTNode* node in allNodes)
	{
		delete node;
	}
}

void UCTManager::onDecisionPoint(UCTNode* rootNode)
{
	if (rootNode == NULL)
	{
		rootNode = new UCTNode(this->maxFriendlyGroups, this->maxEnemyGroups);
	}

	for (int i=0; i<UCTManager::UCT_TOTAL_RUNS; i++)
	{
		//TODO: Could be more effecient and clone instead of generate each loop
		UCTGameState* gameState = this->getCurrentGameState();
		this->actionsTaken.clear();
		this->nodesVisited.clear();

		this->uctRun(rootNode, gameState, false);
	}

	//Do policy run to get set of initial actions with highest Q value
	this->actionsTaken.clear();
	this->nodesVisited.clear();
	UCTGameState* gameState = this->getCurrentGameState();
	this->uctRun(rootNode, gameState, true);
	delete gameState;

	//Note: We can match each action's group IDs with group indexs because of 
	//the deterministic way the initial game state was created for simulation
	for each (UCTAction* action in this->actionsTaken)
	{
		if (action->type == UCTAction::AttackAction)
		{
			UCTAttackAction* attackAction = (UCTAttackAction*)action;
			UnitGroup* myGroup = WorldManager::Instance().myArmyGroups->at(attackAction->myGroupIndex);
			Threat* threatToAttack = WorldManager::Instance().threatVector.at(attackAction->enemyGroupIndex);

			WorldManager::Instance().groupAttackMap[myGroup] = threatToAttack;
		}
		else if (action->type = UCTAction::JoinAction)
		{
			UCTJoinAction* joinAction = (UCTJoinAction*)action;
			std::vector<UnitGroup*>* groupVector = new std::vector<UnitGroup*>();

			for each (int index in joinAction->groupIdVector)
			{
				groupVector->push_back(
					WorldManager::Instance().myArmyGroups->at(index));
			}

			WorldManager::Instance().groupJoinVector.push_back(groupVector);
		}
	}
}

UCTGameState* UCTManager::getCurrentGameState()
{
	int count = 0;
	UCTGameState* newGameState = new UCTGameState();

	for each (UnitGroup* group in *(WorldManager::Instance().myArmyGroups))
	{
		newGameState->myGroups.push_back(new UCTGroup(count, group));
		count++;
	}

	count = 0;
	for each (Threat* threat in WorldManager::Instance().threatVector)
	{
		newGameState->enemyGroups.push_back(new UCTGroup(count, threat));
		count++;
	}

	return newGameState;
}

void UCTManager::uctRun(UCTNode* currentNode, UCTGameState* gameState, bool isPolicyRun)
{
	//If we have reached an end state (one of the forces is destoryed)
	if (gameState->isLeaf())
	{
		//Calculate final reward
		float reward = gameState->getRewardValue();

		//propagate back up tree
		for each (UCTAction* action in this->actionsTaken)
		{
			action->totalReward += reward;
			action->visitCount++;
		}

		for each (UCTNode* node in this->nodesVisited)
		{
			node->visitCount++;
		}
	}
	else
	{
		//Add node to visited list
		this->nodesVisited.push_back(currentNode);

		std::vector<UCTAction*> unexploredActions = std::vector<UCTAction*>();
		std::vector<UCTAction*> actionsToRemove = std::vector<UCTAction*>();
		std::vector<UCTAction*> validActions = std::vector<UCTAction*>();
		UCTAction* actionToExecute = NULL;
		bool hasUnexploredActions = false;

		for each (UCTAction* action in currentNode->possibleActions)
		{
			validActions.push_back(action);
		}

		if (validActions.size() > 5)
		{
			int test = 3;;//TODO: REmove
			test++;
			test = test - 2;
		}

		//Trim actions down to those that can actually be performed
		for each (UCTAction* action in validActions)
		{
			if (! gameState->isValidAction(action))
			{
				actionsToRemove.push_back(action);
			}
		}

		for each (UCTAction* action in actionsToRemove)
		{
			Utils::vectorRemoveElement(&validActions, action);
		}

		//Determine unexplored actions.
		//For simulated actions, we explore them at least a set number of times
		//For non-simulated actions (picking actions before all groups are busy), only explore once
		for each(UCTAction* action in validActions)
		{
			bool triggersSimulation = gameState->willTriggerSimulation(action);

			if (action->visitCount == 0)
			{
				unexploredActions.push_back(action);
			}
			else if (triggersSimulation 
				&& action->visitCount < UCTManager::UCT_PER_ACTION_TRIES) //TODO: Fix
			{
				unexploredActions.push_back(action);
			}
		}

		//If there are unexplored actions (and we are not doing a policy run), randomly pick one
		//otherwise, pick one with highest Q value
		if ((! isPolicyRun) && unexploredActions.size() > 0)
		{
			int choice = rand() % unexploredActions.size();
			actionToExecute = unexploredActions.at(choice);
		}
		else
		{
			actionToExecute = validActions.at(0);
			//TODO: Fix
			double highestQValue = actionToExecute->totalReward / actionToExecute->visitCount;
			
			for each (UCTAction* action in validActions)
			{
				//TODO: Fix
				double qValue = action->totalReward / action->visitCount;

				if (qValue > highestQValue)
				{
					actionToExecute = action;
					highestQValue = qValue;
				}
			}
		}

		//Mark that we have take this action
		this->actionsTaken.push_back(actionToExecute);
		
		//Simulate action if needed
		//Otherwise, just tag busy groups and go to next state
		if (gameState->willTriggerSimulation(actionToExecute))
		{
			//Stop at first simulation point for policy run because we only
			//need the intial set of actions
			if (isPolicyRun)
			{
				return;
			}

			gameState->markGroupsForAction(actionToExecute);
			gameState->simulate();
		}
		else
		{
			gameState->markGroupsForAction(actionToExecute);
		}

		//Determine next node
		UCTNode* nextNode = actionToExecute->resultantNode;
		
		if (nextNode == NULL)
		{
			nextNode = new UCTNode(this->maxFriendlyGroups, this->maxEnemyGroups);
			actionToExecute->resultantNode = nextNode;
		}

		this->uctRun(nextNode, gameState, isPolicyRun);
	}
}

void UCTManager::formMyGroups()
{
const int MAX_CLUSTER_DISTANCE = 100;
	std::map<BWAPI::Unit*, UnitGroup*> unitGroupMap;
	std::set<BWAPI::Unit*> myMen = std::set<BWAPI::Unit*>();
	std::vector<UnitGroup*>* unitGroupVector = new std::vector<UnitGroup*>();

	for each (BWAPI::Unit* unit in BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType().canMove())
		{
			myMen.insert(unit);
		}
	}

	//TODO: Could be much more effecient
	//Determine groups using simple agglomerative hiearchial clustering
	for each (BWAPI::Unit* unit in myMen)
	{
		unitGroupMap[unit] = NULL;
	}

	for each (BWAPI::Unit* unit in myMen)
	{
		if (unitGroupMap[unit] != NULL) {continue;}

		for each (BWAPI::Unit* otherUnit in myMen)
		{
			if (unit == otherUnit) {continue;}

			if (unit->getDistance(otherUnit) < MAX_CLUSTER_DISTANCE)
			{
				if (unitGroupMap[otherUnit] != NULL)
				{
					unitGroupMap[unit] = unitGroupMap[otherUnit];
					unitGroupMap[otherUnit]->addUnit(unit);
				}
				else
				{
					UnitGroup* group = new UnitGroup();
					group->addUnit(unit);
					group->addUnit(otherUnit);

					unitGroupMap[unit] = group;
					unitGroupMap[otherUnit] = group;
					unitGroupVector->push_back(group);
				}

				break;
			}
		}

		if (unitGroupMap[unit] == NULL)
		{
			UnitGroup* group = new UnitGroup();
			group->addUnit(unit);
			unitGroupVector->push_back(group);
		}
	}

	WorldManager::Instance().myArmyGroups = unitGroupVector;
}

UCTManager::~UCTManager(void)
{
}