#include <algorithm>

#include "ArenaTypes.h"
#include "../Utilities/Bytes.h"
#include "../Utilities/Debug.h"

void ArenaTypes::Light::init(const uint8_t *data)
{
	this->x = Bytes::getLE16(data);
	this->y = Bytes::getLE16(data + 2);
	this->radius = Bytes::getLE16(data + 4);
}

void ArenaTypes::MIFHeader::init(const uint8_t *data)
{
	this->unknown1 = *data;
	this->entryCount = *(data + 1);

	const uint16_t *startXStart = reinterpret_cast<const uint16_t*>(data + 2);
	const uint16_t *startXEnd = startXStart + this->startX.size();
	std::copy(startXStart, startXEnd, this->startX.begin());

	const uint16_t *startYStart = startXEnd;
	const uint16_t *startYEnd = startYStart + this->startY.size();
	std::copy(startYStart, startYEnd, this->startY.begin());

	this->startingLevelIndex = *(data + 18);
	this->levelCount = *(data + 19);
	this->unknown2 = *(data + 20);
	this->mapWidth = Bytes::getLE16(data + 21);
	this->mapHeight = Bytes::getLE16(data + 23);

	const uint8_t *unknown3Start = data + 25;
	const uint8_t *unknown3End = unknown3Start + this->unknown3.size();
	std::copy(unknown3Start, unknown3End, this->unknown3.begin());
}

const size_t ArenaTypes::MIFLock::SIZE = 3;

void ArenaTypes::MIFLock::init(const uint8_t *data)
{
	this->x = *data;
	this->y = *(data + 1);
	this->lockLevel = *(data + 2);
}

void ArenaTypes::MIFTarget::init(const uint8_t *data)
{
	this->x = *data;
	this->y = *(data + 1);
}

const size_t ArenaTypes::MIFTrigger::SIZE = 4;

void ArenaTypes::MIFTrigger::init(const uint8_t *data)
{
	this->x = *data;
	this->y = *(data + 1);
	this->textIndex = *(data + 2);
	this->soundIndex = *(data + 3);
}

void ArenaTypes::GameState::init(const uint8_t *data)
{
	DebugNotImplemented();
}

void ArenaTypes::SaveGame::init(const uint8_t *data)
{
	const uint8_t *screenBufferStart = data;
	const uint8_t *screenBufferEnd = screenBufferStart + this->screenBuffer.size();
	std::copy(screenBufferStart, screenBufferEnd, this->screenBuffer.begin());

	const uint8_t *paletteStart = screenBufferEnd;
	const uint8_t *paletteEnd = paletteStart + this->palette.size();
	std::copy(paletteStart, paletteEnd, this->palette.begin());

	const uint8_t *gameStateStart = paletteEnd;
	const uint8_t *gameStateEnd = gameStateStart + 3559;
	this->gameState.init(gameStateStart);

	const uint16_t *gameLevelStart = reinterpret_cast<const uint16_t*>(gameStateEnd);
	const uint16_t *gameLevelEnd = gameLevelStart + this->gameLevel.size();
	std::copy(gameLevelStart, gameLevelEnd, this->gameLevel.begin());
}

void ArenaTypes::SaveEngine::CreatureData::init(const uint8_t *data)
{
	std::copy(data, data + this->unknown.size(), this->unknown.begin());
}

void ArenaTypes::SaveEngine::InventoryItem::init(const uint8_t *data)
{
	this->slotID = *data;
	this->weight = Bytes::getLE16(data + 1);
	this->hands = *(data + 3);
	this->param1 = *(data + 4);
	this->param2 = *(data + 5);
	this->health = Bytes::getLE16(data + 6);
	this->maxHealth = Bytes::getLE16(data + 8);
	this->price = Bytes::getLE32(data + 10);
	this->flags = *(data + 14);
	this->x = *(data + 15);
	this->material = *(data + 16);
	this->y = *(data + 17);
	this->attribute = *(data + 18);
}

const size_t ArenaTypes::SaveEngine::LootItem::SIZE = 28;

void ArenaTypes::SaveEngine::LootItem::init(const uint8_t *data)
{
	this->unknown1 = Bytes::getLE16(data);
	this->containerPosition = Bytes::getLE16(data + 2);
	this->floor = *(data + 4);
	this->goldValue = Bytes::getLE16(data + 5);
	this->unknown2 = Bytes::getLE16(data + 7);

	const uint8_t *inventoryItemStart = data + 8;
	this->inventoryItem.init(inventoryItemStart);
}

const size_t ArenaTypes::SaveEngine::NPCData::SIZE = 1054;

void ArenaTypes::SaveEngine::NPCData::init(const uint8_t *data)
{
	DebugNotImplemented();
}

void ArenaTypes::SaveEngine::CityGenData::init(const uint8_t *data)
{
	const char *cityNameStart = reinterpret_cast<const char*>(data);
	const char *cityNameEnd = cityNameStart + this->cityName.size();
	std::copy(cityNameStart, cityNameEnd, this->cityName.begin());

	const char *mifNameStart = cityNameEnd;
	const char *mifNameEnd = mifNameStart + this->mifName.size();
	std::copy(mifNameStart, mifNameEnd, this->mifName.begin());

	this->citySize = *(data + 33);
	this->blockOffset = Bytes::getLE16(data + 34);
	this->provinceID = *(data + 36);
	this->cityType = *(data + 37);
	this->localX = Bytes::getLE16(data + 38);
	this->localY = Bytes::getLE16(data + 40);
	this->cityID = *(data + 42);
	this->unknown = *(data + 43);
	this->absLatitude = Bytes::getLE16(data + 44);
	this->terrainType = *(data + 46);
	this->quarter = *(data + 47);
	this->rulerSeed = Bytes::getLE32(data + 48);
	this->citySeed = Bytes::getLE32(data + 52);
}

void ArenaTypes::SaveEngine::Buff::init(const uint8_t *data)
{
	std::copy(data, data + this->unknown.size(), this->unknown.begin());
}

void ArenaTypes::SaveEngine::NPCSprite::init(const uint8_t *data)
{
	DebugNotImplemented();
}

const size_t ArenaTypes::SaveEngine::BaseQuest::SIZE = 41;

void ArenaTypes::SaveEngine::BaseQuest::init(const uint8_t *data)
{
	this->questSeed = Bytes::getLE32(data);
	this->location1 = Bytes::getLE16(data + 4);
	this->item1 = Bytes::getLE16(data + 6);
	this->startDate = Bytes::getLE32(data + 8);
	this->dueDate = Bytes::getLE32(data + 12);
	this->tavernData = Bytes::getLE32(data + 16);
	this->destinationData = Bytes::getLE32(data + 20);
	this->reward = Bytes::getLE32(data + 24);
	this->portrait = Bytes::getLE16(data + 28);
	this->questGiverSeed = Bytes::getLE32(data + 30);
	this->opponentFaction = Bytes::getLE16(data + 34);
	this->destinationCityID = *(data + 36);
	this->questCityID = *(data + 37);
	this->unknown = *(data + 38);
	this->relationship = *(data + 39);
	this->escorteeIsFemale = *(data + 40);
}

const size_t ArenaTypes::SaveEngine::ExtQuest::SIZE = 51;

void ArenaTypes::SaveEngine::ExtQuest::init(const uint8_t *data)
{
	const uint8_t *baseQuestStart = data;
	const uint8_t *baseQuestEnd = baseQuestStart + BaseQuest::SIZE;
	this->baseQuest.init(baseQuestStart);

	const uint8_t *unknownStart = baseQuestEnd;
	const uint8_t *unknownEnd = unknownStart + this->unknown.size();
	std::copy(unknownStart, unknownEnd, this->unknown.begin());

	this->faction = *(data + 46);
	this->npcRace = *(data + 47);
	this->monsterRace = *(data + 48);
	this->locationName = *(data + 49);
	this->locNameTemplate = *(data + 50);
}

const size_t ArenaTypes::SaveEngine::MainQuestData::SIZE = 10;

void ArenaTypes::SaveEngine::MainQuestData::init(const uint8_t *data)
{
	this->canHaveVision = *data;
	this->nextStep = *(data + 1);
	this->dungeonLocationKnown = *(data + 2);
	this->acceptedKeyQuest = *(data + 3);
	this->hadVision = *(data + 4);
	this->talkedToRuler = *(data + 5);
	this->stepDone = *(data + 6);
	this->hasKeyItem = *(data + 7);
	this->hasStaffPiece = *(data + 8);
	this->showKey = *(data + 9);
}

const size_t ArenaTypes::SaveEngine::ArtifactQuestData::SIZE = 19;

void ArenaTypes::SaveEngine::ArtifactQuestData::init(const uint8_t *data)
{
	this->currentArtifact = *data;
	this->tavernLocation = Bytes::getLE32(data + 1);
	this->mapDungeonID = *(data + 5);
	this->mapProvinceID = *(data + 6);
	this->artifactDungeonID = *(data + 7);
	this->artifactProvinceID = *(data + 8);
	this->artifactQuestFlags = *(data + 9);
	this->artifactBitMask = Bytes::getLE16(data + 10);
	this->artifactQuestStarted = Bytes::getLE32(data + 12);
	this->artifactDays = *(data + 16);
	this->artifactPriceOrOffset = Bytes::getLE16(data + 17);
}

const size_t ArenaTypes::SaveEngine::PlayerData::SIZE = 2609;

void ArenaTypes::SaveEngine::PlayerData::init(const uint8_t *data)
{
	DebugNotImplemented();
}

void ArenaTypes::SaveEngine::InternalState::init(const uint8_t *data)
{
	std::copy(data, data + this->unknown.size(), this->unknown.begin());
}

void ArenaTypes::SaveEngine::init(const uint8_t *data)
{
	auto unscramble = [](uint8_t *data, size_t length)
	{
		uint16_t buffer = static_cast<uint16_t>(length);
		for (uint16_t i = 0; i < length; i++)
		{
			const uint8_t key = Bytes::ror16(buffer, buffer & 0xF) & 0xFF;
			data[i] ^= key;
			buffer--;
		}
	};

	// Unscramble the first two members (player and player data). Do unscrambling on the
	// incoming bytes so the process is unaffected by struct padding.
	std::array<uint8_t, NPCData::SIZE> playerBuffer;
	const uint8_t *playerBufferStart = data;
	const uint8_t *playerBufferEnd = playerBufferStart + playerBuffer.size();
	std::copy(playerBufferStart, playerBufferEnd, playerBuffer.begin());
	unscramble(playerBuffer.data(), playerBuffer.size());
	this->player.init(playerBuffer.data());

	std::array<uint8_t, PlayerData::SIZE> playerDataBuffer;
	const uint8_t *playerDataBufferStart = playerBufferEnd;
	const uint8_t *playerDataBufferEnd = playerDataBufferStart + playerDataBuffer.size();
	std::copy(playerDataBufferStart, playerDataBufferEnd, playerDataBuffer.begin());
	unscramble(playerDataBuffer.data(), playerDataBuffer.size());
	this->playerData.init(playerDataBuffer.data());

	const uint8_t *enemiesStart = playerDataBufferEnd;
	const uint8_t *enemiesEnd = enemiesStart + (NPCData::SIZE * this->enemies.size());
	for (size_t i = 0; i < this->enemies.size(); i++)
	{
		this->enemies.at(i).init(enemiesStart + (NPCData::SIZE * i));
	}

	const uint8_t *lootStart = enemiesEnd;
	const uint8_t *lootEnd = lootStart + (LootItem::SIZE * this->loot.size());
	for (size_t i = 0; i < this->loot.size(); i++)
	{
		this->loot.at(i).init(lootStart + (LootItem::SIZE * i));
	}

	const uint8_t *gameState2Start = lootEnd;
	this->gameState2.init(gameState2Start);
}

const size_t ArenaTypes::MQLevelState::HashTable::SIZE = 450;

void ArenaTypes::MQLevelState::HashTable::init(const uint8_t *data)
{
	this->triggerCount = *data;

	const uint8_t *triggersStart = data + 1;
	const uint8_t *triggersEnd = triggersStart + (MIFTrigger::SIZE * this->triggers.size());
	for (size_t i = 0; i < this->triggers.size(); i++)
	{
		this->triggers.at(i).init(triggersStart + (MIFTrigger::SIZE * i));
	}

	this->lockCount = *(data + 257);

	const uint8_t *locksStart = data + 258;
	for (size_t i = 0; i < this->locks.size(); i++)
	{
		this->locks.at(i).init(locksStart + (MIFLock::SIZE * i));
	}
}

void ArenaTypes::MQLevelState::init(const uint8_t *data)
{
	const uint8_t *hashTablesStart = data;
	for (size_t i = 0; i < this->hashTables.size(); i++)
	{
		this->hashTables.at(i).init(hashTablesStart + (HashTable::SIZE * i));
	}
}

const size_t ArenaTypes::SpellData::SIZE = 85;

void ArenaTypes::SpellData::init(const uint8_t *data)
{
	for (size_t i = 0; i < this->params.size(); i++)
	{
		const size_t offset = i * 6;
		auto &param = this->params.at(i);
		param.at(0) = Bytes::getLE16(data + offset);
		param.at(1) = Bytes::getLE16(data + offset + 2);
		param.at(2) = Bytes::getLE16(data + offset + 4);
	}

	this->targetType = *(data + 36);
	this->unknown = *(data + 37);
	this->element = *(data + 38);
	this->flags = Bytes::getLE16(data + 39);

	for (size_t i = 0; i < 3; i++)
	{
		this->effects.at(i) = *(data + 41 + i);
		this->subEffects.at(i) = *(data + 44 + i);
		this->affectedAttributes.at(i) = *(data + 47 + i);
	}

	this->cost = Bytes::getLE16(data + 50);

	const char *nameStart = reinterpret_cast<const char*>(data + 52);
	const char *nameEnd = nameStart + this->name.size();
	std::copy(nameStart, nameEnd, this->name.begin());
}

void ArenaTypes::Tavern::init(const uint8_t *data)
{
	this->remainingHours = Bytes::getLE16(data);
	this->timeLimit = Bytes::getLE32(data + 2);
}

const size_t ArenaTypes::Automap::FogOfWarCache::Note::SIZE = 64;

void ArenaTypes::Automap::FogOfWarCache::Note::init(const uint8_t *data)
{
	this->x = Bytes::getLE16(data);
	this->y = Bytes::getLE16(data + 2);

	const char *textStart = reinterpret_cast<const char*>(data + 4);
	const char *textEnd = textStart + this->text.size();
	std::copy(textStart, textEnd, this->text.begin());
}

const size_t ArenaTypes::Automap::FogOfWarCache::SIZE = 8196;

void ArenaTypes::Automap::FogOfWarCache::init(const uint8_t *data)
{
	this->levelHash = Bytes::getLE32(data);

	const uint8_t *notesStart = data + 4;
	const uint8_t *notesEnd = notesStart + (Note::SIZE * this->notes.size());
	for (size_t i = 0; i < this->notes.size(); i++)
	{
		this->notes.at(i).init(notesStart + (Note::SIZE * i));
	}

	const uint8_t *bitmapStart = notesEnd;
	const uint8_t *bitmapEnd = bitmapStart + this->bitmap.size();
	std::copy(bitmapStart, bitmapEnd, this->bitmap.begin());
}

void ArenaTypes::Automap::init(const uint8_t *data)
{
	const uint8_t *cachesStart = data;
	for (size_t i = 0; i < this->caches.size(); i++)
	{
		this->caches.at(i).init(cachesStart + (FogOfWarCache::SIZE * i));
	}
}

void ArenaTypes::Log::init(const uint8_t *data)
{
	DebugNotImplemented();
}
