#include "MoveAction.h"

MoveAction::MoveAction(BWAPI::Unit* unit,  BWAPI::Position position)
{
	this->deleteVectorOnEnd = true;
	this->unitVector = new std::vector<BWAPI::Unit*>();
	this->position = BWAPI::Position(position.x(), position.y());

	this->unitVector->push_back(unit);
}

MoveAction::MoveAction(BWAPI::Unit* unit, int x, int y)
{
	this->deleteVectorOnEnd = true;
	this->unitVector = new std::vector<BWAPI::Unit*>();
	this->position = BWAPI::Position(x, y);

	this->unitVector->push_back(unit);
}

MoveAction::MoveAction(std::vector<BWAPI::Unit*>* unitVector,  BWAPI::Position position)
{
	this->deleteVectorOnEnd = false;
	this->unitVector = unitVector;
	this->position = BWAPI::Position(position.x(), position.y());
}

MoveAction::MoveAction(std::vector<BWAPI::Unit*>* unitVector, int x, int y)
{
	this->deleteVectorOnEnd = true;
	this->unitVector = unitVector;
	this->position = BWAPI::Position(x, y);
}

bool MoveAction::isReady()
{
	return true;
}

bool MoveAction::isStillValid()
{
	return true;
}

void MoveAction::execute()
{
	for each (BWAPI::Unit* unit in (*this->unitVector))
	{
		if (unit->exists())
		{
			unit->move(this->position);
		}
	}
}

std::string MoveAction::toString()
{
	std::string isStillValidText = (this->isStillValid() ? "T" : "F");
	std::string isReadyText = (this->isReady() ? "T" : "F");
	std::string priorityText, toX, toY;
	std::string unitText = "";

	std::stringstream stream;
	stream << this->priority;
	priorityText = stream.str();

	stream.str("");
	stream << this->position.x();
	toX = stream.str();

	stream.clear();
	stream << this->position.y();
	stream.str("");

	
	unitText = this->unitVector->front()->getType().getName();

	if (this->unitVector->size() > 1)
	{
		unitText += "+" + (this->unitVector->size() -1);
	}

	return "[P:" + priorityText+ "]"
		+ "[R:" + isReadyText + "]"
		+ "[V:" + isStillValidText + "]"
		+ " MoveAction"
		+ " " + unitText + " to (" + toX + "," + toY + ")";
}


MoveAction::~MoveAction(void)
{
	if (deleteVectorOnEnd)
	{
		delete this->unitVector;
	}
}