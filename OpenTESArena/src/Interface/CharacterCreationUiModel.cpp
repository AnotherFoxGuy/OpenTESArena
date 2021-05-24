#include "CharacterCreationUiModel.h"
#include "CharacterCreationUiView.h"
#include "../Game/Game.h"
#include "../Items/ArmorMaterial.h"
#include "../Items/ArmorMaterialType.h"
#include "../Items/MetalType.h"
#include "../Items/Shield.h"
#include "../Items/ShieldType.h"
#include "../WorldMap/LocationUtils.h"

#include "components/debug/Debug.h"
#include "components/utilities/String.h"

std::string CharacterCreationUiModel::getPlayerName(Game &game)
{
	const CharacterCreationState &charCreationState = game.getCharacterCreationState();
	return std::string(charCreationState.getName());
}

std::string CharacterCreationUiModel::getPlayerRaceName(Game &game)
{
	const CharacterCreationState &charCreationState = game.getCharacterCreationState();
	const ExeData &exeData = game.getBinaryAssetLibrary().getExeData();

	const auto &singularRaceNames = exeData.races.singularNames;
	const int raceNameIndex = charCreationState.getRaceIndex();
	DebugAssertIndex(singularRaceNames, raceNameIndex);
	return singularRaceNames[raceNameIndex];
}

std::string CharacterCreationUiModel::getPlayerClassName(Game &game)
{
	const CharacterClassLibrary &charClassLibrary = game.getCharacterClassLibrary();
	const CharacterCreationState &charCreationState = game.getCharacterCreationState();
	const int defID = charCreationState.getClassDefID();
	const CharacterClassDefinition &charClassDef = charClassLibrary.getDefinition(defID);
	return charClassDef.getName();
}

std::string CharacterCreationUiModel::getChooseClassCreationTitleText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	std::string text = exeData.charCreation.chooseClassCreation;
	text = String::replace(text, '\r', '\n');
	return text;
}

std::string CharacterCreationUiModel::getGenerateClassButtonText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	return exeData.charCreation.chooseClassCreationGenerate;
}

std::string CharacterCreationUiModel::getGenerateClassButtonTooltipText()
{
	return "Answer questions\n(not implemented)";
}

std::string CharacterCreationUiModel::getSelectClassButtonText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	return exeData.charCreation.chooseClassCreationSelect;
}

std::string CharacterCreationUiModel::getSelectClassButtonTooltipText()
{
	return "Choose from a list";
}

std::string CharacterCreationUiModel::getChooseClassTitleText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	return exeData.charCreation.chooseClassList;
}

std::string CharacterCreationUiModel::getChooseClassArmorTooltipText(const CharacterClassDefinition &charClassDef)
{
	std::vector<int> allowedArmors(charClassDef.getAllowedArmorCount());
	for (int i = 0; i < static_cast<int>(allowedArmors.size()); i++)
	{
		allowedArmors[i] = charClassDef.getAllowedArmor(i);
	}

	std::sort(allowedArmors.begin(), allowedArmors.end());

	std::string armorString;

	// Decide what the armor string says.
	if (allowedArmors.size() == 0)
	{
		armorString = "None";
	}
	else
	{
		int lengthCounter = 0;

		// Collect all allowed armor display names for the class.
		for (int i = 0; i < static_cast<int>(allowedArmors.size()); i++)
		{
			const int materialType = allowedArmors[i];
			auto materialString = ArmorMaterial::typeToString(
				static_cast<ArmorMaterialType>(materialType));
			lengthCounter += static_cast<int>(materialString.size());
			armorString.append(materialString);

			// If not the last element, add a comma.
			if (i < (static_cast<int>(allowedArmors.size()) - 1))
			{
				armorString.append(", ");

				// If too long, add a new line.
				if (lengthCounter > CharacterCreationUiView::MaxTooltipLineLength)
				{
					lengthCounter = 0;
					armorString.append("\n   ");
				}
			}
		}
	}

	armorString.append(".");
	return armorString;
}

std::string CharacterCreationUiModel::getChooseClassShieldTooltipText(const CharacterClassDefinition &charClassDef)
{
	std::vector<int> allowedShields(charClassDef.getAllowedShieldCount());
	for (int i = 0; i < static_cast<int>(allowedShields.size()); i++)
	{
		allowedShields[i] = charClassDef.getAllowedShield(i);
	}

	std::sort(allowedShields.begin(), allowedShields.end());

	std::string shieldsString;

	// Decide what the shield string says.
	if (allowedShields.size() == 0)
	{
		shieldsString = "None";
	}
	else
	{
		int lengthCounter = 0;

		// Collect all allowed shield display names for the class.
		for (int i = 0; i < static_cast<int>(allowedShields.size()); i++)
		{
			const int shieldType = allowedShields[i];
			MetalType dummyMetal = MetalType::Iron;
			auto typeString = Shield(static_cast<ShieldType>(shieldType), dummyMetal).typeToString();
			lengthCounter += static_cast<int>(typeString.size());
			shieldsString.append(typeString);

			// If not the last element, add a comma.
			if (i < (static_cast<int>(allowedShields.size()) - 1))
			{
				shieldsString.append(", ");

				// If too long, add a new line.
				if (lengthCounter > CharacterCreationUiView::MaxTooltipLineLength)
				{
					lengthCounter = 0;
					shieldsString.append("\n   ");
				}
			}
		}
	}

	shieldsString.append(".");
	return shieldsString;
}

std::string CharacterCreationUiModel::getChooseClassWeaponTooltipText(const CharacterClassDefinition &charClassDef, Game &game)
{
	// Get weapon names from the executable.
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	const auto &weaponStrings = exeData.equipment.weaponNames;

	std::vector<int> allowedWeapons(charClassDef.getAllowedWeaponCount());
	for (int i = 0; i < static_cast<int>(allowedWeapons.size()); i++)
	{
		allowedWeapons[i] = charClassDef.getAllowedWeapon(i);
	}

	std::sort(allowedWeapons.begin(), allowedWeapons.end(),
		[&weaponStrings](int a, int b)
	{
		const std::string &aStr = weaponStrings.at(a);
		const std::string &bStr = weaponStrings.at(b);
		return aStr.compare(bStr) < 0;
	});

	std::string weaponsString;

	// Decide what the weapon string says.
	if (allowedWeapons.size() == 0)
	{
		// If the class is allowed zero weapons, it still doesn't exclude fists, I think.
		weaponsString = "None";
	}
	else
	{
		int lengthCounter = 0;

		// Collect all allowed weapon display names for the class.
		for (int i = 0; i < static_cast<int>(allowedWeapons.size()); i++)
		{
			const int weaponID = allowedWeapons.at(i);
			const std::string &weaponName = weaponStrings.at(weaponID);
			lengthCounter += static_cast<int>(weaponName.size());
			weaponsString.append(weaponName);

			// If not the the last element, add a comma.
			if (i < (static_cast<int>(allowedWeapons.size()) - 1))
			{
				weaponsString.append(", ");

				// If too long, add a new line.
				if (lengthCounter > CharacterCreationUiView::MaxTooltipLineLength)
				{
					lengthCounter = 0;
					weaponsString.append("\n   ");
				}
			}
		}
	}

	weaponsString.append(".");
	return weaponsString;
}

std::string CharacterCreationUiModel::getChooseClassFullTooltipText(const CharacterClassDefinition &charClassDef, Game &game)
{
	// Doesn't look like the category name is easy to get from the original data. Potentially could attach something
	// to the char class definition like a bool saying "the class name is also a category name".
	constexpr std::array<const char*, 3> ClassCategoryNames =
	{
		"Mage", "Thief", "Warrior"
	};

	const int categoryIndex = charClassDef.getCategoryID();
	DebugAssertIndex(ClassCategoryNames, categoryIndex);
	const std::string categoryName = ClassCategoryNames[categoryIndex];
	const std::string text = charClassDef.getName() + " (" + categoryName + " class)" + "\n\n" +
		(charClassDef.canCastMagic() ? "Can" : "Cannot") + " cast magic" + "\n" +
		"Health die: " + "d" + std::to_string(charClassDef.getHealthDie()) + "\n" +
		"Armors: " + CharacterCreationUiModel::getChooseClassArmorTooltipText(charClassDef) + "\n" +
		"Shields: " + CharacterCreationUiModel::getChooseClassShieldTooltipText(charClassDef) + "\n" +
		"Weapons: " + CharacterCreationUiModel::getChooseClassWeaponTooltipText(charClassDef, game);

	return text;
}

std::string CharacterCreationUiModel::getChooseGenderTitleText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	return exeData.charCreation.chooseGender;
}

std::string CharacterCreationUiModel::getChooseGenderMaleText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	return exeData.charCreation.chooseGenderMale;
}

std::string CharacterCreationUiModel::getChooseGenderFemaleText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	return exeData.charCreation.chooseGenderFemale;
}

std::string CharacterCreationUiModel::getChooseNameTitleText(Game &game)
{
	const auto &charCreationState = game.getCharacterCreationState();
	const auto &charClassLibrary = game.getCharacterClassLibrary();
	const int charClassDefID = charCreationState.getClassDefID();
	const auto &charClassDef = charClassLibrary.getDefinition(charClassDefID);

	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	std::string text = exeData.charCreation.chooseName;
	text = String::replace(text, "%s", charClassDef.getName());
	return text;
}

bool CharacterCreationUiModel::isPlayerNameCharacterAccepted(char c)
{
	// Only letters and spaces are allowed.
	return (c == ' ') || ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
}

std::string CharacterCreationUiModel::getChooseRaceTitleText(Game &game)
{
	const auto &binaryAssetLibrary = game.getBinaryAssetLibrary();
	const auto &exeData = binaryAssetLibrary.getExeData();
	std::string text = exeData.charCreation.chooseRace;
	text = String::replace(text, '\r', '\n');

	const auto &charCreationState = game.getCharacterCreationState();
	const auto &charClassLibrary = game.getCharacterClassLibrary();
	const int charClassDefID = charCreationState.getClassDefID();
	const auto &charClassDef = charClassLibrary.getDefinition(charClassDefID);

	// Replace first "%s" with player name.
	size_t index = text.find("%s");
	text.replace(index, 2, charCreationState.getName());

	// Replace second "%s" with character class.
	index = text.find("%s");
	text.replace(index, 2, charClassDef.getName());

	return text;
}

std::string CharacterCreationUiModel::getChooseRaceProvinceConfirmTitleText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	std::string text = exeData.charCreation.confirmRace;
	text = String::replace(text, '\r', '\n');

	const auto &charCreationState = game.getCharacterCreationState();
	const int raceIndex = charCreationState.getRaceIndex();

	const auto &charCreationProvinceNames = exeData.locations.charCreationProvinceNames;
	DebugAssertIndex(charCreationProvinceNames, raceIndex);
	const std::string &provinceName = charCreationProvinceNames[raceIndex];

	const auto &pluralRaceNames = exeData.races.pluralNames;
	DebugAssertIndex(pluralRaceNames, raceIndex);
	const std::string &pluralRaceName = pluralRaceNames[raceIndex];

	// Replace first %s with province name.
	size_t index = text.find("%s");
	text.replace(index, 2, provinceName);

	// Replace second %s with plural race name.
	index = text.find("%s");
	text.replace(index, 2, pluralRaceName);

	return text;
}

std::string CharacterCreationUiModel::getChooseRaceProvinceTooltipText(Game &game, int provinceID)
{
	// Get the race name associated with the province.
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	const auto &pluralNames = exeData.races.pluralNames;
	DebugAssertIndex(pluralNames, provinceID);
	const std::string &raceName = pluralNames[provinceID];
	return "Land of the " + raceName;
}

std::optional<int> CharacterCreationUiModel::getChooseRaceProvinceID(Game &game, const Int2 &originalPosition)
{
	const auto &worldMapMasks =game.getBinaryAssetLibrary().getWorldMapMasks();
	const int maskCount = static_cast<int>(worldMapMasks.size());
	for (int maskID = 0; maskID < maskCount; maskID++)
	{
		// Ignore the center province and the "Exit" button.
		constexpr int exitButtonID = 9;
		if ((maskID == LocationUtils::CENTER_PROVINCE_ID) || (maskID == exitButtonID))
		{
			continue;
		}

		const WorldMapMask &mapMask = worldMapMasks.at(maskID);
		const Rect &maskRect = mapMask.getRect();

		if (maskRect.contains(originalPosition))
		{
			// See if the pixel is set in the bitmask.
			const bool success = mapMask.get(originalPosition.x, originalPosition.y);

			if (success)
			{
				// Return the mask's ID.
				return maskID;
			}
		}
	}

	// No province mask found at the given location.
	return std::nullopt;
}

std::string CharacterCreationUiModel::getChooseAttributesText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	std::string text = exeData.charCreation.distributeClassPoints;
	text = String::replace(text, '\r', '\n');
	return text;
}

std::string CharacterCreationUiModel::getAttributesMessageBoxTitleText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	return exeData.charCreation.chooseAttributes;
}

std::string CharacterCreationUiModel::getAttributesMessageBoxSaveText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	std::string text = exeData.charCreation.chooseAttributesSave;

	// @todo: use the formatting characters in the string for color.
	// - For now, just delete them.
	text.erase(1, 2);

	return text;
}

std::string CharacterCreationUiModel::getAttributesMessageBoxRerollText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	std::string text = exeData.charCreation.chooseAttributesReroll;

	// @todo: use the formatting characters in the string for color.
	// - For now, just delete them.
	text.erase(1, 2);

	return text;
}

std::string CharacterCreationUiModel::getAppearanceMessageBoxText(Game &game)
{
	const auto &exeData = game.getBinaryAssetLibrary().getExeData();
	std::string text = exeData.charCreation.chooseAppearance;
	text = String::replace(text, '\r', '\n');
	return text;
}
