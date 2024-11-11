#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <functional>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <vector>

#include "Jolt/Jolt.h"
#include "Jolt/Physics/PhysicsSystem.h"

#include "Clock.h"
#include "Date.h"
#include "../Assets/ArenaTypes.h"
#include "../Assets/BinaryAssetLibrary.h"
#include "../Entities/Player.h"
#include "../Interface/ProvinceMapUiModel.h"
#include "../Math/Random.h"
#include "../Math/Vector2.h"
#include "../Weather/WeatherDefinition.h"
#include "../Weather/WeatherInstance.h"
#include "../World/MapDefinition.h"
#include "../WorldMap/WorldMapInstance.h"

// Intended to be a container for the player and world data that is currently active 
// while a player is loaded (i.e., not in the main menu).

// The GameState object will be initialized only upon loading of the player, and 
// will be uninitialized when the player goes to the main menu (thus unloading
// the character resources). Whichever entry points into the "game" there are, they
// need to load data into the game state object.

class BinaryAssetLibrary;
class CharacterClassLibrary;
class CityDataFile;
class EntityDefinitionLibrary;
class FontLibrary;
class INFFile;
class LocationDefinition;
class LocationInstance;
class MIFFile;
class MusicDefinition;
class ProvinceDefinition;
class Renderer;
class TextAssetLibrary;
class Texture;
class TextureManager;

enum class MapType;

struct RenderCamera;

class GameState
{
public:
	using SceneChangeMusicFunc = std::function<const MusicDefinition*(Game&)>;

	// Used with the currently selected world map location.
	struct WorldMapLocationIDs
	{
		int provinceID;
		int locationID;

		WorldMapLocationIDs(int provinceID, int locationID);
	};
private:
	// Determines length of a real-time second in-game. For the original game, one real second is
	// twenty in-game seconds.
	static constexpr double GAME_TIME_SCALE = static_cast<double>(Clock::SECONDS_IN_A_DAY) / 4320.0;

	MapDefinition activeMapDef;
	int activeLevelIndex;
	MapDefinition prevMapDef; // Stored map definition when in an interior.
	std::optional<CoordInt3> prevMapReturnCoord; // Used when leaving interiors.

	// Scene transition request variables.
	MapDefinition nextMapDef;
	std::optional<CoordInt2> nextMapStartCoord; // Overrides map definition start points; used with city -> wilderness transition.
	VoxelInt2 nextMapPlayerStartOffset; // Used with interior level changes and random dungeons.
	std::optional<WorldMapLocationIDs> nextMapDefLocationIDs;
	std::optional<WeatherDefinition> nextMapDefWeatherDef; // Used with fast travel, etc..
	bool nextMapClearsPrevious; // Clears any previously-loaded map defs (such as when fast travelling).
	int nextLevelIndex;
	SceneChangeMusicFunc nextMusicFunc, nextJingleMusicFunc; // Music changes after a map change.
	
	// Player's current world map location data.
	WorldMapDefinition worldMapDef;
	WorldMapInstance worldMapInst;
	int provinceIndex;
	int locationIndex;
	std::optional<ProvinceMapUiModel::TravelData> travelData; // Non-null when a destination is selected.

	// Game world interface display texts have an associated time remaining. These values are stored here so
	// they are not destroyed when switching away from the game world panel.
	// - Trigger text: lore message from voxel trigger
	// - Action text: description of the player's current action
	// - Effect text: effect on the player (disease, drunk, silence, etc.)
	double triggerTextRemainingSeconds, actionTextRemainingSeconds, effectTextRemainingSeconds;

	// One weather for each of the 36 province quadrants (updated hourly).
	static constexpr int WORLD_MAP_WEATHER_QUADRANT_COUNT = 36;
	ArenaTypes::WeatherType worldMapWeathers[WORLD_MAP_WEATHER_QUADRANT_COUNT];

	// Custom function for *LEVELUP voxel enter events. If no function is set, the default
	// behavior is to decrement the world's level index.
	std::function<void(Game&)> onLevelUpVoxelEnter;

	Date date;
	Clock clock;
	double chasmAnimSeconds;
	bool isCamping;

	WeatherDefinition weatherDef;
	WeatherInstance weatherInst;

	void clearMaps();
public:
	// Creates incomplete game state with no active world, to be further initialized later.
	GameState();
	GameState(GameState&&) = default;
	~GameState();

	void init(ArenaRandom &random);
	
	void clearSession();

	bool hasPendingLevelIndexChange() const;
	bool hasPendingMapDefChange() const;
	bool hasPendingSceneChange() const;

	void queueLevelIndexChange(int newLevelIndex, const VoxelInt2 &playerStartOffset);
	void queueMapDefChange(MapDefinition &&newMapDef, const std::optional<CoordInt2> &startCoord = std::nullopt,
		const std::optional<CoordInt3> &returnCoord = std::nullopt, const VoxelInt2 &playerStartOffset = VoxelInt2::Zero,
		const std::optional<WorldMapLocationIDs> &worldMapLocationIDs = std::nullopt,
		bool clearPreviousMap = false, const std::optional<WeatherDefinition> &weatherDef = std::nullopt);
	void queueMapDefPop();
	void queueMusicOnSceneChange(const SceneChangeMusicFunc &musicFunc, const SceneChangeMusicFunc &jingleMusicFunc = SceneChangeMusicFunc());

	MapType getActiveMapType() const;
	bool isActiveMapValid() const; // Basically "is there something we can populate the scene with?".
	int getActiveLevelIndex() const;
	int getActiveSkyIndex() const;
	const MapDefinition &getActiveMapDef() const;
	double getActiveCeilingScale() const;
	bool isActiveMapNested() const; // True if the active interior is inside an exterior.
	const WorldMapDefinition &getWorldMapDefinition() const;
	const ProvinceDefinition &getProvinceDefinition() const;
	const LocationDefinition &getLocationDefinition() const;
	WorldMapInstance &getWorldMapInstance();
	ProvinceInstance &getProvinceInstance();
	LocationInstance &getLocationInstance();
	const ProvinceMapUiModel::TravelData *getTravelData() const;
	BufferView<const ArenaTypes::WeatherType> getWorldMapWeathers() const;
	ArenaTypes::WeatherType getWeatherForLocation(int provinceIndex, int locationIndex) const;
	Date &getDate();
	Clock &getClock();
	double getDayPercent() const;

	// Gets a percentage representing the current progress through the looping chasm animation.
	double getChasmAnimPercent() const;

	// Gets the currently selected weather and associated state.
	const WeatherDefinition &getWeatherDefinition() const;
	const WeatherInstance &getWeatherInstance() const;

	// Refers to fog in outdoor dungeons and daytime fog, not the heavy fog screen effect.
	bool isFogActive() const;

	// Gets the custom function for the *LEVELUP voxel enter event.
	std::function<void(Game&)> &getOnLevelUpVoxelEnter();

	// On-screen text is visible if it has remaining duration.
	bool triggerTextIsVisible() const;
	bool actionTextIsVisible() const;
	bool effectTextIsVisible() const;

	// Sets whether the player is camping, which influences time passing and other things.
	void setIsCamping(bool isCamping);

	// Sets the player's world map travel data when they select a destination.
	void setTravelData(std::optional<ProvinceMapUiModel::TravelData> travelData);

	// Sets on-screen text duration for various types of in-game messages.
	void setTriggerTextDuration(const std::string_view &text);
	void setActionTextDuration(const std::string_view &text);
	void setEffectTextDuration(const std::string_view &text);

	// Resets on-screen text boxes to empty and hidden.
	void resetTriggerTextDuration();
	void resetActionTextDuration();
	void resetEffectTextDuration();

	// Recalculates the weather for each global quarter (done hourly).
	void updateWeatherList(ArenaRandom &random, const ExeData &exeData);

	// Applies any pending scene transition, setting the new level active in the game world and renderer.
	void applyPendingSceneChange(Game &game, JPH::PhysicsSystem &physicsSystem, double dt);

	void tickGameClock(double dt, Game &game);
	void tickChasmAnimation(double dt);
	void tickSky(double dt, Game &game);
	void tickWeather(double dt, Game &game);
	void tickUiMessages(double dt);
	void tickPlayerAttack(double dt, Game &game);
	void tickVoxels(double dt, Game &game);
	void tickEntities(double dt, Game &game);
	void tickCollision(double dt, JPH::PhysicsSystem &physicsSystem, Game &game);
	void tickPlayerMovementTriggers(const CoordDouble3 &oldPlayerCoord, const CoordDouble3 &newPlayerCoord, Game &game);
	void tickVisibility(const RenderCamera &renderCamera, Game &game);
	void tickRendering(const RenderCamera &renderCamera, Game &game);
};

#endif
