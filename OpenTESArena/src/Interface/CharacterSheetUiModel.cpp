#include <vector>

#include "CharacterSheetUiModel.h"
#include "../Entities/CharacterClassLibrary.h"
#include "../Entities/PrimaryAttribute.h"
#include "../Game/Game.h"

#include "components/debug/Debug.h"

std::string CharacterSheetUiModel::getPlayerName(Game &game)
{
	const Player &player = game.player;
	return player.displayName;
}

std::string CharacterSheetUiModel::getPlayerRaceName(Game &game)
{
	const Player &player = game.player;
	const ExeData &exeData = BinaryAssetLibrary::getInstance().getExeData();

	const auto &singularRaceNames = exeData.races.singularNames;
	const int raceNameIndex = player.raceID;
	DebugAssertIndex(singularRaceNames, raceNameIndex);
	return singularRaceNames[raceNameIndex];
}

std::string CharacterSheetUiModel::getPlayerClassName(Game &game)
{
	const CharacterClassLibrary &charClassLibrary = CharacterClassLibrary::getInstance();
	const Player &player = game.player;
	const int defID = player.charClassDefID;
	const CharacterClassDefinition &charClassDef = charClassLibrary.getDefinition(defID);
	return charClassDef.getName();
}

std::vector<PrimaryAttribute> CharacterSheetUiModel::getPlayerAttributes(Game &game)
{
	const Player &player = game.player;
	return player.attributes.getAll();
}
