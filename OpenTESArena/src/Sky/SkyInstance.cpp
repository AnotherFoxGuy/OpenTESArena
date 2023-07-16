#include <cmath>

#include "ArenaSkyUtils.h"
#include "SkyAirDefinition.h"
#include "SkyDefinition.h"
#include "SkyInfoDefinition.h"
#include "SkyInstance.h"
#include "SkyLandDefinition.h"
#include "SkyMoonDefinition.h"
#include "SkyStarDefinition.h"
#include "SkySunDefinition.h"
#include "SkyUtils.h"
#include "../Assets/ArenaPaletteName.h"
#include "../Assets/TextureManager.h"
#include "../Math/Random.h"
#include "../Rendering/RendererUtils.h"
#include "../Weather/WeatherInstance.h"
#include "../World/MapDefinition.h"
#include "../World/MapType.h"

#include "components/debug/Debug.h"

SkyObjectInstance::SkyObjectInstance()
{
	this->width = 0.0;
	this->height = 0.0;
	this->textureType = static_cast<SkyObjectTextureType>(-1);
	this->textureAssetEntryID = -1;
	this->paletteIndexEntryID = -1;
	this->emissive = false;
	this->animIndex = -1;
	this->pivotType = static_cast<SkyObjectPivotType>(-1);
}

void SkyObjectInstance::init(const Double3 &baseDirection, double width, double height,
	ObjectTextureID objectTextureID, bool emissive)
{
	this->baseDirection = baseDirection;
	this->transformedDirection = Double3::Zero;
	this->width = width;
	this->height = height;
	this->objectTextureID = objectTextureID;
	this->emissive = emissive;
	this->animIndex = -1;
	this->pivotType = static_cast<SkyObjectPivotType>(-1);
}

SkyObjectAnimationInstance::SkyObjectAnimationInstance(Buffer<TextureAsset> &&textureAssets, double targetSeconds)
	: textureAssets(std::move(textureAssets))
{
	this->targetSeconds = targetSeconds;
	this->currentSeconds = 0.0;
}

bool SkyInstance::tryGetTextureAssetEntryID(BufferView<const TextureAsset> textureAssets, SkyObjectTextureAssetEntryID *outID) const
{
	for (int i = 0; i < static_cast<int>(this->textureAssetEntries.size()); i++)
	{
		const SkyObjectTextureAssetEntry &entry = this->textureAssetEntries[i];
		const BufferView<const TextureAsset> entryTextureAssets(entry.textureAssets);
		if (entryTextureAssets.getCount() != textureAssets.getCount())
		{
			continue;
		}

		bool success = true;
		for (int j = 0; j < textureAssets.getCount(); j++)
		{
			const TextureAsset &textureAsset = textureAssets[j];
			const TextureAsset &entryTextureAsset = entryTextureAssets[j];
			if (textureAsset != entryTextureAsset)
			{
				success = false;
				break;
			}
		}

		if (!success)
		{
			continue;
		}

		*outID = static_cast<SkyObjectTextureAssetEntryID>(i);
		return true;
	}

	return false;
}

bool SkyInstance::tryGetPaletteIndexEntryID(uint8_t paletteIndex, SkyObjectPaletteIndexEntryID *outID) const
{
	for (int i = 0; i < static_cast<int>(this->paletteIndexEntries.size()); i++)
	{
		const SkyObjectPaletteIndexEntry &entry = this->paletteIndexEntries[i];
		if (entry.paletteIndex != paletteIndex)
		{
			continue;
		}

		*outID = static_cast<SkyObjectPaletteIndexEntryID>(i);
		return true;
	}

	return false;
}

SkyInstance::SkyInstance()
{
	this->landStart = -1;
	this->landEnd = -1;
	this->airStart = -1;
	this->airEnd = -1;
	this->moonStart = -1;
	this->moonEnd = -1;
	this->sunStart = -1;
	this->sunEnd = -1;
	this->starStart = -1;
	this->starEnd = -1;
	this->lightningStart = -1;
	this->lightningEnd = -1;
}

void SkyInstance::init(const SkyDefinition &skyDefinition, const SkyInfoDefinition &skyInfoDefinition,
	int currentDay, TextureManager &textureManager)
{
	auto addGeneralObjectInst = [this, &textureManager](const Double3 &baseDirection, const TextureAsset &textureAsset, bool emissive)
	{
		// @todo: should this search for the texture asset entry or should it be a parameter?
		// - also need to get the dimensions at that same index, so might be useful to have a "get texture asset entry index" function.
		DebugNotImplemented();

		const ScopedObjectTextureRef &objectTextureRef = cacheIter->objectTextureRef;
		const ObjectTextureID objectTextureID = objectTextureRef.get();
		const int textureWidth = objectTextureRef.getWidth();
		const int textureHeight = objectTextureRef.getHeight();

		double width, height;
		SkyUtils::getSkyObjectDimensions(textureWidth, textureHeight, &width, &height);

		ObjectInstance objectInst;
		objectInst.init(baseDirection, width, height, objectTextureID, emissive);
		this->objectInsts.emplace_back(std::move(objectInst));
	};

	auto addSmallStarObjectInst = [this, &renderer](const Double3 &baseDirection, uint8_t paletteIndex)
	{
		const auto cacheIter = std::find_if(this->loadedSmallStarTextures.begin(), this->loadedSmallStarTextures.end(),
			[paletteIndex](const LoadedSmallStarTextureEntry &entry)
		{
			return entry.paletteIndex == paletteIndex;
		});

		if (cacheIter == this->loadedSmallStarTextures.end())
		{
			DebugCrash("Expected small star texture with color \"" + std::to_string(paletteIndex) + "\" to be loaded.");
		}

		const ScopedObjectTextureRef &objectTextureRef = cacheIter->objectTextureRef;
		const ObjectTextureID objectTextureID = objectTextureRef.get();
		const int textureWidth = objectTextureRef.getWidth();
		const int textureHeight = objectTextureRef.getHeight();

		double width, height;
		SkyUtils::getSkyObjectDimensions(textureWidth, textureHeight, &width, &height);

		constexpr bool emissive = true;
		ObjectInstance objectInst;
		objectInst.init(baseDirection, width, height, objectTextureID, emissive);
		this->objectInsts.emplace_back(std::move(objectInst));
	};

	auto addAnimInst = [this](int objectIndex, BufferView<const TextureAsset> textureAssets, double targetSeconds)
	{
		// It is assumed that only general sky objects (not small stars) can have animations, and that their
		// textures have already been loaded earlier in SkyInstance::init().
		Buffer<ObjectTextureID> objectTextureIDs(textureAssets.getCount());
		for (int i = 0; i < textureAssets.getCount(); i++)
		{
			const TextureAsset &textureAsset = textureAssets.get(i);
			const auto cacheIter = std::find_if(this->loadedSkyObjectTextures.begin(), this->loadedSkyObjectTextures.end(),
				[&textureAsset](const LoadedSkyObjectTextureEntry &entry)
			{
				return entry.textureAsset == textureAsset;
			});

			DebugAssert(cacheIter != this->loadedSkyObjectTextures.end());
			const ObjectTextureID objectTextureID = cacheIter->objectTextureRef.get();
			objectTextureIDs.set(i, objectTextureID);
		}

		this->animInsts.emplace_back(objectIndex, std::move(objectTextureIDs), targetSeconds);
	};

	// Spawn all sky objects from the ready-to-bake format. Any animated objects start on their first frame.
	int landInstCount = 0;
	for (int i = 0; i < skyDefinition.getLandPlacementDefCount(); i++)
	{
		const SkyDefinition::LandPlacementDef &placementDef = skyDefinition.getLandPlacementDef(i);
		const SkyDefinition::LandDefID defID = placementDef.id;
		const SkyLandDefinition &skyLandDef = skyInfoDefinition.getLand(defID);

		// Load all textures for this land (mostly meant for volcanoes).
		for (const TextureAsset &textureAsset : skyLandDef.textureAssets)
		{
			loadGeneralSkyObjectTexture(textureAsset);
		}

		const TextureAsset &firstTextureAsset = skyLandDef.textureAssets.get(0);
		for (const Radians position : placementDef.positions)
		{
			// Convert radians to direction.
			const Radians angleY = 0.0;
			const Double3 direction = SkyUtils::getSkyObjectDirection(position, angleY);
			const bool emissive = skyLandDef.shadingType == SkyLandShadingType::Bright;
			addGeneralObjectInst(direction, firstTextureAsset, emissive);

			// Only land objects support animations (for now).
			if (skyLandDef.hasAnimation)
			{
				const int objectIndex = static_cast<int>(this->objectInsts.size()) - 1;
				BufferView<const TextureAsset> textureAssets(skyLandDef.textureAssets);
				const double targetSeconds = static_cast<double>(textureAssets.getCount()) * ArenaSkyUtils::ANIMATED_LAND_SECONDS_PER_FRAME;
				addAnimInst(objectIndex, textureAssets, targetSeconds);
			}

			// Do position transform since it's only needed once at initialization for land objects.
			ObjectInstance &objectInst = this->objectInsts.back();
			objectInst.setTransformedDirection(objectInst.getBaseDirection());
		}

		landInstCount += static_cast<int>(placementDef.positions.size());
	}

	this->landStart = 0;
	this->landEnd = this->landStart + landInstCount;

	int airInstCount = 0;
	for (int i = 0; i < skyDefinition.getAirPlacementDefCount(); i++)
	{
		const SkyDefinition::AirPlacementDef &placementDef = skyDefinition.getAirPlacementDef(i);
		const SkyDefinition::AirDefID defID = placementDef.id;
		const SkyAirDefinition &skyAirDef = skyInfoDefinition.getAir(defID);
		const TextureAsset &textureAsset = skyAirDef.textureAsset;
		loadGeneralSkyObjectTexture(textureAsset);

		for (const std::pair<Radians, Radians> &position : placementDef.positions)
		{
			// Convert X and Y radians to direction.
			const Radians angleX = position.first;
			const Radians angleY = position.second;
			const Double3 direction = SkyUtils::getSkyObjectDirection(angleX, angleY);
			constexpr bool emissive = false;
			addGeneralObjectInst(direction, textureAsset, emissive);

			// Do position transform since it's only needed once at initialization for air objects.
			ObjectInstance &objectInst = this->objectInsts.back();
			objectInst.setTransformedDirection(objectInst.getBaseDirection());
		}

		airInstCount += static_cast<int>(placementDef.positions.size());
	}

	this->airStart = this->landEnd;
	this->airEnd = this->airStart + airInstCount;

	int moonInstCount = 0;
	for (int i = 0; i < skyDefinition.getMoonPlacementDefCount(); i++)
	{
		const SkyDefinition::MoonPlacementDef &placementDef = skyDefinition.getMoonPlacementDef(i);
		const SkyDefinition::MoonDefID defID = placementDef.id;
		const SkyMoonDefinition &skyMoonDef = skyInfoDefinition.getMoon(defID);

		// Get the image from the current day.
		DebugAssert(skyMoonDef.textureAssets.getCount() > 0);
		const TextureAsset &textureAsset = skyMoonDef.textureAssets.get(currentDay);
		loadGeneralSkyObjectTexture(textureAsset);

		for (const SkyDefinition::MoonPlacementDef::Position &position : placementDef.positions)
		{
			// Default to the direction at midnight here, biased by the moon's bonus latitude and
			// orbit percent.
			// @todo: not sure this matches the original game but it looks fine.
			const Matrix4d moonLatitudeRotation = RendererUtils::getLatitudeRotation(position.bonusLatitude);
			const Matrix4d moonOrbitPercentRotation = Matrix4d::xRotation(position.orbitPercent * Constants::TwoPi);
			const Double3 baseDirection = -Double3::UnitY;
			Double4 direction4D(baseDirection.x, baseDirection.y, baseDirection.z, 0.0);
			direction4D = moonLatitudeRotation * direction4D;
			direction4D = moonOrbitPercentRotation * direction4D;

			constexpr bool emissive = true;
			addGeneralObjectInst(Double3(direction4D.x, direction4D.y, direction4D.z), textureAsset, emissive);
		}

		moonInstCount += static_cast<int>(placementDef.positions.size());
	}

	this->moonStart = this->airEnd;
	this->moonEnd = this->moonStart + moonInstCount;

	int sunInstCount = 0;
	for (int i = 0; i < skyDefinition.getSunPlacementDefCount(); i++)
	{
		const SkyDefinition::SunPlacementDef &placementDef = skyDefinition.getSunPlacementDef(i);
		const SkyDefinition::SunDefID defID = placementDef.id;
		const SkySunDefinition &skySunDef = skyInfoDefinition.getSun(defID);
		const TextureAsset &textureAsset = skySunDef.textureAsset;
		loadGeneralSkyObjectTexture(textureAsset);

		for (const double position : placementDef.positions)
		{
			// Default to the direction at midnight here, biased by the sun's bonus latitude.
			const Matrix4d sunLatitudeRotation = RendererUtils::getLatitudeRotation(position);
			const Double3 baseDirection = -Double3::UnitY;
			const Double4 direction4D = sunLatitudeRotation *
				Double4(baseDirection.x, baseDirection.y, baseDirection.z, 0.0);
			constexpr bool emissive = true;
			addGeneralObjectInst(Double3(direction4D.x, direction4D.y, direction4D.z), textureAsset, emissive);
		}

		sunInstCount += static_cast<int>(placementDef.positions.size());
	}

	this->sunStart = this->moonEnd;
	this->sunEnd = this->sunStart + sunInstCount;

	int starInstCount = 0;
	for (int i = 0; i < skyDefinition.getStarPlacementDefCount(); i++)
	{
		const SkyDefinition::StarPlacementDef &placementDef = skyDefinition.getStarPlacementDef(i);
		const SkyDefinition::StarDefID defID = placementDef.id;
		const SkyStarDefinition &skyStarDef = skyInfoDefinition.getStar(defID);

		// @todo: this is where the texture-id-from-texture-manager design is breaking, and getting
		// a renderer texture handle would be better. SkyInstance::init() should be able to allocate
		// textures IDs from the renderer eventually, and look up cached ones by string.
		const SkyStarType starType = skyStarDef.type;
		if (starType == SkyStarType::Small)
		{
			// Small stars are 1x1 pixels.
			const SkySmallStarDefinition &smallStar = skyStarDef.smallStar;
			const uint8_t paletteIndex = smallStar.paletteIndex;
			loadSmallStarTexture(paletteIndex);

			for (const Double3 &position : placementDef.positions)
			{
				// Use star direction directly.
				addSmallStarObjectInst(position, paletteIndex);
			}
		}
		else if (starType == SkyStarType::Large)
		{
			const SkyLargeStarDefinition &largeStar = skyStarDef.largeStar;
			const TextureAsset &textureAsset = largeStar.textureAsset;
			loadGeneralSkyObjectTexture(textureAsset);

			for (const Double3 &position : placementDef.positions)
			{
				// Use star direction directly.
				constexpr bool emissive = true;
				addGeneralObjectInst(position, textureAsset, emissive);
			}
		}
		else
		{
			DebugNotImplementedMsg(std::to_string(static_cast<int>(starType)));
		}

		starInstCount += static_cast<int>(placementDef.positions.size());
	}

	this->starStart = this->sunEnd;
	this->starEnd = this->starStart + starInstCount;

	// Populate lightning bolt assets for random selection.
	const int lightningBoltDefCount = skyInfoDefinition.getLightningCount();
	if (lightningBoltDefCount > 0)
	{
		this->lightningAnimIndices.init(lightningBoltDefCount);

		for (int i = 0; i < lightningBoltDefCount; i++)
		{
			const SkyLightningDefinition &skyLightningDef = skyInfoDefinition.getLightning(i);

			// Load all textures for this lightning bolt.
			for (const TextureAsset &textureAsset : skyLightningDef.textureAssets)
			{
				loadGeneralSkyObjectTexture(textureAsset);
			}

			const TextureAsset &firstTextureAsset = skyLightningDef.textureAssets.get(0);
			addGeneralObjectInst(Double3::Zero, firstTextureAsset, true);
			
			BufferView<const TextureAsset> textureAssets(skyLightningDef.textureAssets);
			addAnimInst(static_cast<int>(this->objectInsts.size()) - 1, textureAssets, skyLightningDef.animSeconds);

			this->lightningAnimIndices.set(i, static_cast<int>(this->animInsts.size()) - 1);
		}

		this->lightningStart = this->starEnd;
		this->lightningEnd = this->lightningStart + lightningBoltDefCount;
	}
}

int SkyInstance::getLandStartIndex() const
{
	return this->landStart;
}

int SkyInstance::getLandEndIndex() const
{
	return this->landEnd;
}

int SkyInstance::getAirStartIndex() const
{
	return this->airStart;
}

int SkyInstance::getAirEndIndex() const
{
	return this->airEnd;
}

int SkyInstance::getStarStartIndex() const
{
	return this->starStart;
}

int SkyInstance::getStarEndIndex() const
{
	return this->starEnd;
}

int SkyInstance::getSunStartIndex() const
{
	return this->sunStart;
}

int SkyInstance::getSunEndIndex() const
{
	return this->sunEnd;
}

int SkyInstance::getMoonStartIndex() const
{
	return this->moonStart;
}

int SkyInstance::getMoonEndIndex() const
{
	return this->moonEnd;
}

int SkyInstance::getLightningStartIndex() const
{
	return this->lightningStart;
}

int SkyInstance::getLightningEndIndex() const
{
	return this->lightningEnd;
}

bool SkyInstance::isLightningVisible(int objectIndex) const
{
	return this->currentLightningBoltObjectIndex == objectIndex;
}

void SkyInstance::getSkyObject(int index, Double3 *outDirection, ObjectTextureID *outObjectTextureID,
	bool *outEmissive, double *outWidth, double *outHeight) const
{
	DebugAssertIndex(this->objectInsts, index);
	const ObjectInstance &objectInst = this->objectInsts[index];
	*outDirection = objectInst.getTransformedDirection();
	*outObjectTextureID = objectInst.getObjectTextureID();
	*outEmissive = objectInst.isEmissive();
	*outWidth = objectInst.getWidth();
	*outHeight = objectInst.getHeight();
}

std::optional<double> SkyInstance::tryGetObjectAnimPercent(int index) const
{
	// See if the object has an animation.
	for (int i = 0; i < static_cast<int>(this->animInsts.size()); i++)
	{
		const SkyInstance::AnimInstance &animInst = this->animInsts[i];
		if (animInst.objectIndex == index)
		{
			return animInst.currentSeconds / animInst.targetSeconds;
		}
	}

	// No animation for the object.
	return std::nullopt;
}

void SkyInstance::update(double dt, double latitude, double daytimePercent, const WeatherInstance &weatherInst,
	Random &random, const TextureManager &textureManager)
{
	// Update lightning (if any).
	if (weatherInst.hasRain())
	{
		const WeatherRainInstance &rainInst = weatherInst.getRain();
		const std::optional<WeatherRainInstance::Thunderstorm> &thunderstorm = rainInst.thunderstorm;
		if (thunderstorm.has_value() && thunderstorm->active)
		{
			DebugAssert(this->lightningAnimIndices.getCount() > 0);

			const std::optional<double> lightningBoltPercent = thunderstorm->getLightningBoltPercent();
			const bool visibilityChanged = this->currentLightningBoltObjectIndex.has_value() != lightningBoltPercent.has_value();
			if (visibilityChanged && lightningBoltPercent.has_value())
			{
				const int lightningGroupCount = this->lightningEnd - this->lightningStart;
				this->currentLightningBoltObjectIndex = this->lightningStart + random.next(lightningGroupCount);

				// Pick a new spot for the chosen lightning bolt.
				const Radians lightningAngleX = thunderstorm->lightningBoltAngle;
				const Double3 lightningDirection = SkyUtils::getSkyObjectDirection(lightningAngleX, 0.0);

				ObjectInstance &lightningObjInst = this->objectInsts[*this->currentLightningBoltObjectIndex];
				lightningObjInst.setTransformedDirection(lightningDirection);
			}

			if (lightningBoltPercent.has_value())
			{
				const int animInstIndex = this->lightningAnimIndices.get(*this->currentLightningBoltObjectIndex - this->lightningStart);
				AnimInstance &lightningAnimInst = this->animInsts[animInstIndex];
				lightningAnimInst.currentSeconds = *lightningBoltPercent * lightningAnimInst.targetSeconds;
			}
			else
			{
				this->currentLightningBoltObjectIndex = std::nullopt;
			}
		}
		else
		{
			this->currentLightningBoltObjectIndex = std::nullopt;
		}
	}

	// Update animations.
	const int animInstCount = static_cast<int>(this->animInsts.size());
	for (int i = 0; i < animInstCount; i++)
	{
		AnimInstance &animInst = this->animInsts[i];

		// Don't update if it's an inactive lightning bolt.
		const bool isLightningAnim = [this, i]()
		{
			if (this->lightningAnimIndices.getCount() == 0)
			{
				return false;
			}

			const auto iter = std::find(this->lightningAnimIndices.begin(), this->lightningAnimIndices.end(), i);
			return iter != this->lightningAnimIndices.end();
		}();

		if (isLightningAnim && !this->isLightningVisible(animInst.objectIndex))
		{
			continue;
		}

		animInst.currentSeconds += dt;
		if (animInst.currentSeconds >= animInst.targetSeconds)
		{
			animInst.currentSeconds = std::fmod(animInst.currentSeconds, animInst.targetSeconds);
		}

		const int imageCount = animInst.objectTextureIDs.getCount();
		const double animPercent = animInst.currentSeconds / animInst.targetSeconds;
		const int animIndex = std::clamp(static_cast<int>(static_cast<double>(imageCount) * animPercent), 0, imageCount - 1);
		const ObjectTextureID newObjectTextureID = animInst.objectTextureIDs.get(animIndex);

		DebugAssertIndex(this->objectInsts, animInst.objectIndex);
		ObjectInstance &objectInst = this->objectInsts[animInst.objectIndex];
		objectInst.setObjectTextureID(newObjectTextureID);
	}

	const Matrix4d timeOfDayRotation = RendererUtils::getTimeOfDayRotation(daytimePercent);
	const Matrix4d latitudeRotation = RendererUtils::getLatitudeRotation(latitude);

	auto transformObjectsInRange = [this, &timeOfDayRotation, &latitudeRotation](int start, int end)
	{
		for (int i = start; i < end; i++)
		{
			DebugAssertIndex(this->objectInsts, i);
			ObjectInstance &objectInst = this->objectInsts[i];
			const Double3 baseDirection = objectInst.getBaseDirection();
			Double4 dir(baseDirection.x, baseDirection.y, baseDirection.z, 0.0);
			dir = timeOfDayRotation * dir;
			dir = latitudeRotation * dir;

			// @temp: flip X and Z.
			// @todo: figure out why. Distant stars should rotate counter-clockwise when facing south,
			// and the sun and moons should rise from the west.
			objectInst.setTransformedDirection(Double3(-dir.x, dir.y, -dir.z));
		}
	};

	// Update transformed sky positions of moons, suns, and stars.
	transformObjectsInRange(this->moonStart, this->moonEnd);
	transformObjectsInRange(this->sunStart, this->sunEnd);
	transformObjectsInRange(this->starStart, this->starEnd);
}

void SkyInstance::clear()
{
	this->textureAssetEntries.clear();
	this->paletteIndexEntries.clear();
	this->objectInsts.clear();
	this->animInsts.clear();
	this->landStart = -1;
	this->landEnd = -1;
	this->airStart = -1;
	this->airEnd = -1;
	this->moonStart = -1;
	this->moonEnd = -1;
	this->sunStart = -1;
	this->sunEnd = -1;
	this->starStart = -1;
	this->starEnd = -1;
	this->lightningStart = -1;
	this->lightningEnd = -1;
}
