#ifndef RENDER_SKY_MANAGER_H
#define RENDER_SKY_MANAGER_H

#include "RenderDrawCall.h"
#include "RenderGeometryUtils.h"
#include "RenderTextureUtils.h"
#include "../Assets/TextureAsset.h"

class ExeData;
class Renderer;
class SkyInfoDefinition;
class SkyInstance;
class TextureManager;
class WeatherInstance;

enum class WeatherType;

class RenderSkyManager
{
private:
	struct LoadedGeneralSkyObjectTextureEntry
	{
		TextureAsset textureAsset;
		ScopedObjectTextureRef objectTextureRef;

		void init(const TextureAsset &textureAsset, ScopedObjectTextureRef &&objectTextureRef);
	};

	struct LoadedSmallStarTextureEntry
	{
		uint8_t paletteIndex;
		ScopedObjectTextureRef objectTextureRef;

		void init(uint8_t paletteIndex, ScopedObjectTextureRef &&objectTextureRef);
	};

	// All the possible sky color textures to choose from, dependent on the active weather. These are used by
	// the renderer to look up palette colors.
	ScopedObjectTextureRef skyGradientAMTextureRef;
	ScopedObjectTextureRef skyGradientPMTextureRef;
	ScopedObjectTextureRef skyFogTextureRef;
	Buffer<ScopedObjectTextureRef> skyThunderstormTextureRefs; // One for each frame of flash animation.
	ScopedObjectTextureRef skyInteriorTextureRef; // Default black for interiors.

	VertexBufferID bgVertexBufferID;
	AttributeBufferID bgNormalBufferID;
	AttributeBufferID bgTexCoordBufferID;
	IndexBufferID bgIndexBufferID;
	RenderDrawCall bgDrawCall;

	// All sky objects share simple vertex + attribute + index buffers.
	VertexBufferID objectVertexBufferID;
	AttributeBufferID objectNormalBufferID;
	AttributeBufferID objectTexCoordBufferID;
	IndexBufferID objectIndexBufferID;
	std::vector<LoadedGeneralSkyObjectTextureEntry> generalSkyObjectTextures;
	std::vector<LoadedSmallStarTextureEntry> smallStarTextures;
	std::vector<RenderDrawCall> objectDrawCalls; // Order matters: stars, sun, planets, clouds, mountains.

	ObjectTextureID getGeneralSkyObjectTextureID(const TextureAsset &textureAsset) const;
	ObjectTextureID getSmallStarTextureID(uint8_t paletteIndex) const;

	void freeBgBuffers(Renderer &renderer);
	void freeObjectBuffers(Renderer &renderer);

	void updateGameWorldPalette(WeatherType weatherType, bool isInterior, double daytimePercent,
		ScopedObjectTextureRef &gameWorldPaletteTextureRef, TextureManager &textureManager, Renderer &renderer);
public:
	RenderSkyManager();

	void init(const ExeData &exeData, TextureManager &textureManager, Renderer &renderer);
	void shutdown(Renderer &renderer);

	const RenderDrawCall &getBgDrawCall() const;
	BufferView<const RenderDrawCall> getObjectDrawCalls() const;

	void loadScene(const SkyInfoDefinition &skyInfoDef, TextureManager &textureManager, Renderer &renderer);
	void update(const SkyInstance &skyInst, WeatherType weatherType, const WeatherInstance &weatherInst,
		const CoordDouble3 &cameraCoord, bool isInterior, double daytimePercent, bool isFoggy, double distantAmbientPercent,
		ScopedObjectTextureRef &gameWorldPaletteTextureRef, TextureManager &textureManager, Renderer &renderer);
	void unloadScene(Renderer &renderer);
};

#endif
