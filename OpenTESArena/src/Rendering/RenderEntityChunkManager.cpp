#include <algorithm>
#include <array>

#include "RenderEntityChunkManager.h"
#include "Renderer.h"
#include "RenderTransform.h"
#include "RenderVoxelChunkManager.h"
#include "../Assets/TextureManager.h"
#include "../Entities/EntityChunkManager.h"
#include "../Entities/EntityVisibilityState.h"
#include "../Voxels/DoorUtils.h"
#include "../Voxels/VoxelChunkManager.h"

#include "components/debug/Debug.h"

namespace
{
	// Creates a buffer of texture refs, intended to be accessed with linearized keyframe indices.
	Buffer<ScopedObjectTextureRef> MakeEntityAnimationTextures(const EntityAnimationDefinition &animDef,
		TextureManager &textureManager, Renderer &renderer)
	{
		Buffer<ScopedObjectTextureRef> textureRefs(animDef.keyframeCount);

		// Need to go by state + keyframe list because the keyframes don't know whether they're mirrored.
		int writeIndex = 0;
		for (int i = 0; i < animDef.stateCount; i++)
		{
			const EntityAnimationDefinitionState &defState = animDef.states[i];
			for (int j = 0; j < defState.keyframeListCount; j++)
			{
				const int keyframeListIndex = defState.keyframeListsIndex + j;
				DebugAssertIndex(animDef.keyframeLists, keyframeListIndex);
				const EntityAnimationDefinitionKeyframeList &keyframeList = animDef.keyframeLists[keyframeListIndex];
				for (int k = 0; k < keyframeList.keyframeCount; k++)
				{
					const int keyframeIndex = keyframeList.keyframesIndex + k;
					DebugAssertIndex(animDef.keyframes, keyframeIndex);
					const EntityAnimationDefinitionKeyframe &keyframe = animDef.keyframes[keyframeIndex];
					const TextureAsset &textureAsset = keyframe.textureAsset;

					const std::optional<TextureBuilderID> textureBuilderID = textureManager.tryGetTextureBuilderID(textureAsset);
					if (!textureBuilderID.has_value())
					{
						DebugLogWarning("Couldn't load entity anim texture \"" + textureAsset.filename + "\".");
						continue;
					}

					const TextureBuilder &textureBuilder = textureManager.getTextureBuilderHandle(*textureBuilderID);
					const int textureWidth = textureBuilder.getWidth();
					const int textureHeight = textureBuilder.getHeight();
					const int texelCount = textureWidth * textureHeight;
					constexpr int bytesPerTexel = 1;
					DebugAssert(textureBuilder.getType() == TextureBuilderType::Paletted);

					ObjectTextureID textureID;
					if (!renderer.tryCreateObjectTexture(textureWidth, textureHeight, bytesPerTexel, &textureID))
					{
						DebugLogWarning("Couldn't create entity anim texture \"" + textureAsset.filename + "\".");
						continue;
					}

					ScopedObjectTextureRef textureRef(textureID, renderer);

					const TextureBuilder::PalettedTexture &palettedTexture = textureBuilder.getPaletted();
					const uint8_t *srcTexels = palettedTexture.texels.begin();

					LockedTexture lockedTexture = renderer.lockObjectTexture(textureID);
					uint8_t *dstTexels = static_cast<uint8_t*>(lockedTexture.texels);

					// Copy texels from source texture, mirroring if necessary.
					for (int y = 0; y < textureHeight; y++)
					{
						for (int x = 0; x < textureWidth; x++)
						{
							const int srcX = !keyframeList.isMirrored ? x : (textureWidth - 1 - x);
							const int srcIndex = srcX + (y * textureWidth);
							const int dstIndex = x + (y * textureWidth);
							dstTexels[dstIndex] = srcTexels[srcIndex];
						}
					}

					renderer.unlockObjectTexture(textureID);
					textureRefs.set(writeIndex, std::move(textureRef));
					writeIndex++;
				}
			}
		}

		DebugAssert(writeIndex == textureRefs.getCount());
		return textureRefs;
	}

	ScopedObjectTextureRef MakeEntityPaletteIndicesTextureRef(const PaletteIndices &paletteIndices, Renderer &renderer)
	{
		const int textureWidth = static_cast<int>(paletteIndices.size());
		constexpr int textureHeight = 1;
		constexpr int bytesPerTexel = 1;

		ObjectTextureID textureID;
		if (!renderer.tryCreateObjectTexture(textureWidth, textureHeight, bytesPerTexel, &textureID))
		{
			DebugCrash("Couldn't create entity palette indices texture.");
		}

		LockedTexture lockedTexture = renderer.lockObjectTexture(textureID);
		uint8_t *dstTexels = static_cast<uint8_t*>(lockedTexture.texels);
		std::copy(paletteIndices.begin(), paletteIndices.end(), dstTexels);
		renderer.unlockObjectTexture(textureID);
		return ScopedObjectTextureRef(textureID, renderer);
	}
}

void RenderEntityChunkManager::LoadedEntityAnimation::init(EntityDefID defID, Buffer<ScopedObjectTextureRef> &&textureRefs)
{
	this->defID = defID;
	this->textureRefs = std::move(textureRefs);
}

RenderEntityChunkManager::RenderEntityChunkManager()
{
	
}

void RenderEntityChunkManager::init(Renderer &renderer)
{
	// Populate entity mesh buffers. All entities share the same buffers, and the normals buffer is updated every frame.
	constexpr int positionComponentsPerVertex = MeshUtils::POSITION_COMPONENTS_PER_VERTEX;
	constexpr int normalComponentsPerVertex = MeshUtils::NORMAL_COMPONENTS_PER_VERTEX;
	constexpr int texCoordComponentsPerVertex = MeshUtils::TEX_COORDS_PER_VERTEX;
	constexpr int entityMeshVertexCount = 4;
	constexpr int entityMeshIndexCount = 6;

	if (!renderer.tryCreateVertexBuffer(entityMeshVertexCount, positionComponentsPerVertex, &this->entityMeshDef.vertexBufferID))
	{
		DebugLogError("Couldn't create vertex buffer for entity mesh ID.");
		return;
	}

	if (!renderer.tryCreateAttributeBuffer(entityMeshVertexCount, normalComponentsPerVertex, &this->entityMeshDef.normalBufferID))
	{
		DebugLogError("Couldn't create normal attribute buffer for entity mesh def.");
		this->entityMeshDef.freeBuffers(renderer);
		return;
	}

	if (!renderer.tryCreateAttributeBuffer(entityMeshVertexCount, texCoordComponentsPerVertex, &this->entityMeshDef.texCoordBufferID))
	{
		DebugLogError("Couldn't create tex coord attribute buffer for entity mesh def.");
		this->entityMeshDef.freeBuffers(renderer);
		return;
	}

	if (!renderer.tryCreateIndexBuffer(entityMeshIndexCount, &this->entityMeshDef.indexBufferID))
	{
		DebugLogError("Couldn't create index buffer for entity mesh def.");
		this->entityMeshDef.freeBuffers(renderer);
		return;
	}

	constexpr std::array<double, entityMeshVertexCount * positionComponentsPerVertex> entityVertices =
	{
		0.0, 1.0, -0.50,
		0.0, 0.0, -0.50,
		0.0, 0.0, 0.50,
		0.0, 1.0, 0.50
	};

	constexpr std::array<double, entityMeshVertexCount * normalComponentsPerVertex> dummyEntityNormals =
	{
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0
	};

	constexpr std::array<double, entityMeshVertexCount * texCoordComponentsPerVertex> entityTexCoords =
	{
		0.0, 0.0,
		0.0, 1.0,
		1.0, 1.0,
		1.0, 0.0
	};

	constexpr std::array<int32_t, entityMeshIndexCount> entityIndices =
	{
		0, 1, 2,
		2, 3, 0
	};

	renderer.populateVertexBuffer(this->entityMeshDef.vertexBufferID, entityVertices);
	renderer.populateAttributeBuffer(this->entityMeshDef.normalBufferID, dummyEntityNormals);
	renderer.populateAttributeBuffer(this->entityMeshDef.texCoordBufferID, entityTexCoords);
	renderer.populateIndexBuffer(this->entityMeshDef.indexBufferID, entityIndices);
}

void RenderEntityChunkManager::shutdown(Renderer &renderer)
{
	for (int i = static_cast<int>(this->activeChunks.size()) - 1; i >= 0; i--)
	{
		ChunkPtr &chunkPtr = this->activeChunks[i];
		this->recycleChunk(i);
	}

	for (const auto &pair : this->entityTransformBufferIDs)
	{
		const UniformBufferID entityTransformBufferID = pair.second;
		renderer.freeUniformBuffer(entityTransformBufferID);
	}

	this->entityTransformBufferIDs.clear();

	this->entityAnims.clear();
	this->entityMeshDef.freeBuffers(renderer);
	this->entityPaletteIndicesTextureRefs.clear();
	this->entityDrawCallsCache.clear();
}

ObjectTextureID RenderEntityChunkManager::getEntityTextureID(EntityInstanceID entityInstID, const CoordDouble2 &cameraCoordXZ,
	const EntityChunkManager &entityChunkManager) const
{
	const EntityInstance &entityInst = entityChunkManager.getEntity(entityInstID);
	const EntityDefID entityDefID = entityInst.defID;
	const auto defIter = std::find_if(this->entityAnims.begin(), this->entityAnims.end(),
		[entityDefID](const LoadedEntityAnimation &loadedAnim)
	{
		return loadedAnim.defID == entityDefID;
	});

	DebugAssertMsg(defIter != this->entityAnims.end(), "Expected loaded entity animation for def ID " + std::to_string(entityDefID) + ".");

	EntityVisibilityState2D visState;
	entityChunkManager.getEntityVisibilityState2D(entityInstID, cameraCoordXZ, visState);

	const EntityDefinition &entityDef = entityChunkManager.getEntityDef(entityDefID);
	const EntityAnimationDefinition &animDef = entityDef.getAnimDef();
	const int linearizedKeyframeIndex = animDef.getLinearizedKeyframeIndex(visState.stateIndex, visState.angleIndex, visState.keyframeIndex);
	const Buffer<ScopedObjectTextureRef> &textureRefs = defIter->textureRefs;
	return textureRefs.get(linearizedKeyframeIndex).get();
}

BufferView<const RenderDrawCall> RenderEntityChunkManager::getEntityDrawCalls() const
{
	return BufferView<const RenderDrawCall>(this->entityDrawCallsCache);
}

void RenderEntityChunkManager::loadEntityTextures(const EntityChunk &entityChunk, const EntityChunkManager &entityChunkManager,
	TextureManager &textureManager, Renderer &renderer)
{
	for (const EntityInstanceID entityInstID : entityChunk.entityIDs)
	{
		const EntityInstance &entityInst = entityChunkManager.getEntity(entityInstID);
		const EntityDefID entityDefID = entityInst.defID;

		const auto animIter = std::find_if(this->entityAnims.begin(), this->entityAnims.end(),
			[entityDefID](const LoadedEntityAnimation &loadedAnim)
		{
			return loadedAnim.defID == entityDefID;
		});

		if (animIter == this->entityAnims.end())
		{
			const EntityDefinition &entityDef = entityChunkManager.getEntityDef(entityDefID);
			const EntityAnimationDefinition &animDef = entityDef.getAnimDef();
			Buffer<ScopedObjectTextureRef> textureRefs = MakeEntityAnimationTextures(animDef, textureManager, renderer);

			LoadedEntityAnimation loadedEntityAnim;
			loadedEntityAnim.init(entityDefID, std::move(textureRefs));
			this->entityAnims.emplace_back(std::move(loadedEntityAnim));
		}

		if (entityInst.isCitizen())
		{
			const EntityPaletteIndicesInstanceID paletteIndicesInstID = entityInst.paletteIndicesInstID;
			const auto paletteIndicesIter = this->entityPaletteIndicesTextureRefs.find(paletteIndicesInstID);
			if (paletteIndicesIter == this->entityPaletteIndicesTextureRefs.end())
			{
				const PaletteIndices &paletteIndices = entityChunkManager.getEntityPaletteIndices(paletteIndicesInstID);
				ScopedObjectTextureRef paletteIndicesTextureRef = MakeEntityPaletteIndicesTextureRef(paletteIndices, renderer);
				this->entityPaletteIndicesTextureRefs.emplace(paletteIndicesInstID, std::move(paletteIndicesTextureRef));
			}
		}
	}
}

void RenderEntityChunkManager::loadEntityUniformBuffers(const EntityChunk &entityChunk, Renderer &renderer)
{
	for (const EntityInstanceID entityInstID : entityChunk.entityIDs)
	{
		DebugAssert(this->entityTransformBufferIDs.find(entityInstID) == this->entityTransformBufferIDs.end());

		// Each entity has a uniform buffer.
		UniformBufferID entityTransformBufferID;
		if (!renderer.tryCreateUniformBuffer(1, sizeof(RenderTransform), alignof(RenderTransform), &entityTransformBufferID))
		{
			DebugLogError("Couldn't create uniform buffer for entity transform.");
			return;
		}

		// Initialize to default transform; it gets updated each frame.
		RenderTransform renderTransform;
		renderTransform.preScaleTranslation = Double3::Zero;
		renderTransform.rotation = Matrix4d::identity();
		renderTransform.scale = Matrix4d::identity();
		renderer.populateUniformBuffer(entityTransformBufferID, renderTransform);

		this->entityTransformBufferIDs.emplace(entityInstID, entityTransformBufferID);
	}
}

void RenderEntityChunkManager::addEntityDrawCall(const Double3 &position, UniformBufferID transformBufferID, int transformIndex,
	ObjectTextureID textureID0, const std::optional<ObjectTextureID> &textureID1, BufferView<const RenderLightID> lightIDs,
	PixelShaderType pixelShaderType, std::vector<RenderDrawCall> &drawCalls)
{
	RenderDrawCall drawCall;
	drawCall.position = position;
	drawCall.transformBufferID = transformBufferID;
	drawCall.transformIndex = transformIndex;
	drawCall.vertexBufferID = this->entityMeshDef.vertexBufferID;
	drawCall.normalBufferID = this->entityMeshDef.normalBufferID;
	drawCall.texCoordBufferID = this->entityMeshDef.texCoordBufferID;
	drawCall.indexBufferID = this->entityMeshDef.indexBufferID;
	drawCall.textureIDs[0] = textureID0;
	drawCall.textureIDs[1] = textureID1;
	drawCall.textureSamplingType0 = TextureSamplingType::Default;
	drawCall.textureSamplingType1 = TextureSamplingType::Default;
	drawCall.lightingType = RenderLightingType::PerPixel;
	drawCall.lightPercent = 0.0;

	DebugAssert(std::size(drawCall.lightIDs) >= lightIDs.getCount());
	std::copy(lightIDs.begin(), lightIDs.end(), std::begin(drawCall.lightIDs));
	drawCall.lightIdCount = lightIDs.getCount();

	drawCall.vertexShaderType = VertexShaderType::Entity;
	drawCall.pixelShaderType = pixelShaderType;
	drawCall.pixelShaderParam0 = 0.0;

	drawCalls.emplace_back(std::move(drawCall));
}

void RenderEntityChunkManager::rebuildEntityChunkDrawCalls(RenderEntityChunk &renderChunk, const EntityChunk &entityChunk,
	const RenderVoxelChunk &renderVoxelChunk, const CoordDouble2 &cameraCoordXZ, double ceilingScale,
	const VoxelChunkManager &voxelChunkManager, const EntityChunkManager &entityChunkManager)
{
	renderChunk.entityDrawCalls.clear();

	for (const EntityInstanceID entityInstID : entityChunk.entityIDs)
	{
		const EntityInstance &entityInst = entityChunkManager.getEntity(entityInstID);
		const CoordDouble2 &entityCoord = entityChunkManager.getEntityPosition(entityInst.positionID);
		const EntityDefinition &entityDef = entityChunkManager.getEntityDef(entityInst.defID);

		// Get visibility state for true Y position (@todo: separate position from the anim index values).
		EntityVisibilityState3D visState;
		entityChunkManager.getEntityVisibilityState3D(entityInstID, cameraCoordXZ, ceilingScale, voxelChunkManager, visState);

		// Convert entity XYZ to world space.
		const Double3 worldPos = VoxelUtils::coordToWorldPoint(visState.flatPosition);

		const ObjectTextureID textureID0 = this->getEntityTextureID(entityInstID, cameraCoordXZ, entityChunkManager);
		std::optional<ObjectTextureID> textureID1 = std::nullopt;
		PixelShaderType pixelShaderType = PixelShaderType::AlphaTested;

		const bool isCitizen = entityInst.isCitizen();
		const bool isGhost = EntityUtils::isGhost(entityDef);
		if (isCitizen)
		{
			const EntityPaletteIndicesInstanceID paletteIndicesInstID = entityInst.paletteIndicesInstID;
			const auto paletteIndicesIter = this->entityPaletteIndicesTextureRefs.find(paletteIndicesInstID);
			DebugAssertMsg(paletteIndicesIter != this->entityPaletteIndicesTextureRefs.end(), "Expected entity palette indices texture for ID " + std::to_string(paletteIndicesInstID) + ".");
			textureID1 = paletteIndicesIter->second.get();
			pixelShaderType = PixelShaderType::AlphaTestedWithPaletteIndexLookup;
		}
		else if (isGhost)
		{
			pixelShaderType = PixelShaderType::AlphaTestedWithLightLevelOpacity;
		}

		const VoxelDouble3 &entityLightPoint = visState.flatPosition.point; // Where the entity receives its light (can't use center due to some really tall entities reaching outside the chunk).
		const VoxelInt3 entityLightVoxel = VoxelUtils::pointToVoxel(entityLightPoint, ceilingScale);
		BufferView<const RenderLightID> lightIdsView; // Limitation of reusing lights per voxel: entity is unlit if they are outside the world.
		if (renderVoxelChunk.isValidVoxel(entityLightVoxel.x, entityLightVoxel.y, entityLightVoxel.z))
		{
			const RenderVoxelLightIdList &voxelLightIdList = renderVoxelChunk.voxelLightIdLists.get(entityLightVoxel.x, entityLightVoxel.y, entityLightVoxel.z);
			lightIdsView = voxelLightIdList.getLightIDs();
		}

		const auto transformBufferIter = this->entityTransformBufferIDs.find(entityInstID);
		DebugAssert(transformBufferIter != this->entityTransformBufferIDs.end());
		const UniformBufferID entityTransformBufferID = transformBufferIter->second;
		const int entityTransformIndex = 0; // Each entity has their own transform buffer.
		this->addEntityDrawCall(worldPos, entityTransformBufferID, entityTransformIndex, textureID0, textureID1,
			lightIdsView, pixelShaderType, renderChunk.entityDrawCalls);
	}
}

void RenderEntityChunkManager::rebuildEntityDrawCallsList()
{
	this->entityDrawCallsCache.clear();

	// @todo: eventually this should sort by distance from a CoordDouble2
	for (size_t i = 0; i < this->activeChunks.size(); i++)
	{
		const ChunkPtr &chunkPtr = this->activeChunks[i];
		BufferView<const RenderDrawCall> entityDrawCalls = chunkPtr->entityDrawCalls;
		this->entityDrawCallsCache.insert(this->entityDrawCallsCache.end(), entityDrawCalls.begin(), entityDrawCalls.end());
	}
}

void RenderEntityChunkManager::updateActiveChunks(BufferView<const ChunkInt2> newChunkPositions, BufferView<const ChunkInt2> freedChunkPositions,
	const VoxelChunkManager &voxelChunkManager, Renderer &renderer)
{
	for (const ChunkInt2 &chunkPos : freedChunkPositions)
	{
		const int chunkIndex = this->getChunkIndex(chunkPos);
		this->recycleChunk(chunkIndex);
	}

	for (const ChunkInt2 &chunkPos : newChunkPositions)
	{
		const VoxelChunk &voxelChunk = voxelChunkManager.getChunkAtPosition(chunkPos);

		const int spawnIndex = this->spawnChunk();
		RenderEntityChunk &renderChunk = this->getChunkAtIndex(spawnIndex);
		renderChunk.init(chunkPos, voxelChunk.getHeight());
	}

	// Free any unneeded chunks for memory savings in case the chunk distance was once large
	// and is now small. This is significant even for chunk distance 2->1, or 25->9 chunks.
	this->chunkPool.clear();
}

void RenderEntityChunkManager::updateEntities(BufferView<const ChunkInt2> activeChunkPositions,
	BufferView<const ChunkInt2> newChunkPositions, const CoordDouble2 &cameraCoordXZ, const VoxelDouble2 &cameraDirXZ,
	double ceilingScale, const VoxelChunkManager &voxelChunkManager, const EntityChunkManager &entityChunkManager,
	const RenderVoxelChunkManager &renderVoxelChunkManager, TextureManager &textureManager, Renderer &renderer)
{
	for (const EntityInstanceID entityInstID : entityChunkManager.getQueuedDestroyEntityIDs())
	{
		const EntityInstance &entityInst = entityChunkManager.getEntity(entityInstID);
		if (entityInst.isCitizen())
		{
			const EntityPaletteIndicesInstanceID paletteIndicesInstID = entityInst.paletteIndicesInstID;
			const auto paletteIndicesIter = this->entityPaletteIndicesTextureRefs.find(paletteIndicesInstID);
			if (paletteIndicesIter != this->entityPaletteIndicesTextureRefs.end())
			{
				this->entityPaletteIndicesTextureRefs.erase(paletteIndicesIter);
			}
		}

		const auto transformBufferIter = this->entityTransformBufferIDs.find(entityInstID);
		DebugAssert(transformBufferIter != this->entityTransformBufferIDs.end());
		const UniformBufferID entityTransformBufferID = transformBufferIter->second;
		renderer.freeUniformBuffer(entityTransformBufferID);
		this->entityTransformBufferIDs.erase(transformBufferIter);
	}

	for (const ChunkInt2 &chunkPos : newChunkPositions)
	{
		RenderEntityChunk &renderChunk = this->getChunkAtPosition(chunkPos);
		const EntityChunk &entityChunk = entityChunkManager.getChunkAtPosition(chunkPos);
		this->loadEntityTextures(entityChunk, entityChunkManager, textureManager, renderer);
		this->loadEntityUniformBuffers(entityChunk, renderer);
	}

	// The rotation for entities so they face the camera.
	const Radians allEntitiesRotationRadians = -MathUtils::fullAtan2(cameraDirXZ) - Constants::HalfPi;
	const Matrix4d allEntitiesRotationMatrix = Matrix4d::yRotation(allEntitiesRotationRadians);

	for (const ChunkInt2 &chunkPos : activeChunkPositions)
	{
		RenderEntityChunk &renderChunk = this->getChunkAtPosition(chunkPos);
		const EntityChunk &entityChunk = entityChunkManager.getChunkAtPosition(chunkPos);
		const RenderVoxelChunk &renderVoxelChunk = renderVoxelChunkManager.getChunkAtPosition(chunkPos);

		// Update entity render transforms.
		for (const EntityInstanceID entityInstID : entityChunk.entityIDs)
		{
			const EntityInstance &entityInst = entityChunkManager.getEntity(entityInstID);
			const CoordDouble2 &entityCoord = entityChunkManager.getEntityPosition(entityInst.positionID);
			const EntityDefinition &entityDef = entityChunkManager.getEntityDef(entityInst.defID);
			const EntityAnimationDefinition &animDef = entityDef.getAnimDef();

			// Get visibility state for animation index values (@todo: separate vis state from the true Y position calculation).
			EntityVisibilityState3D visState;
			entityChunkManager.getEntityVisibilityState3D(entityInstID, cameraCoordXZ, ceilingScale, voxelChunkManager, visState);
			const int linearizedKeyframeIndex = animDef.getLinearizedKeyframeIndex(visState.stateIndex, visState.angleIndex, visState.keyframeIndex);
			DebugAssertIndex(animDef.keyframes, linearizedKeyframeIndex);
			const EntityAnimationDefinitionKeyframe &keyframe = animDef.keyframes[linearizedKeyframeIndex];

			// Convert entity XYZ to world space.
			const Double3 worldPos = VoxelUtils::coordToWorldPoint(visState.flatPosition);
			const Matrix4d scaleMatrix = Matrix4d::scale(1.0, keyframe.height, keyframe.width);

			const auto transformBufferIter = this->entityTransformBufferIDs.find(entityInstID);
			DebugAssert(transformBufferIter != this->entityTransformBufferIDs.end());
			const UniformBufferID entityTransformBufferID = transformBufferIter->second;

			RenderTransform entityRenderTransform;
			entityRenderTransform.preScaleTranslation = Double3::Zero;
			entityRenderTransform.rotation = allEntitiesRotationMatrix;
			entityRenderTransform.scale = Matrix4d::scale(1.0, keyframe.height, keyframe.width);
			renderer.populateUniformBuffer(entityTransformBufferID, entityRenderTransform);
		}

		this->rebuildEntityChunkDrawCalls(renderChunk, entityChunk, renderVoxelChunk, cameraCoordXZ, ceilingScale,
			voxelChunkManager, entityChunkManager);
	}

	this->rebuildEntityDrawCallsList();

	// Update normals buffer.
	const VoxelDouble2 entityDir = -cameraDirXZ;
	constexpr int entityMeshVertexCount = 4;
	const std::array<double, entityMeshVertexCount * MeshUtils::NORMAL_COMPONENTS_PER_VERTEX> entityNormals =
	{
		entityDir.x, 0.0, entityDir.y,
		entityDir.x, 0.0, entityDir.y,
		entityDir.x, 0.0, entityDir.y,
		entityDir.x, 0.0, entityDir.y
	};

	renderer.populateAttributeBuffer(this->entityMeshDef.normalBufferID, entityNormals);
}

void RenderEntityChunkManager::cleanUp()
{
	
}

void RenderEntityChunkManager::unloadScene(Renderer &renderer)
{
	for (const auto &pair : this->entityTransformBufferIDs)
	{
		const UniformBufferID transformBufferID = pair.second;
		renderer.freeUniformBuffer(transformBufferID);
	}

	this->entityTransformBufferIDs.clear();
	this->entityAnims.clear();
	this->entityPaletteIndicesTextureRefs.clear();

	for (int i = static_cast<int>(this->activeChunks.size()) - 1; i >= 0; i--)
	{
		ChunkPtr &chunkPtr = this->activeChunks[i];
		this->recycleChunk(i);
	}

	this->entityDrawCallsCache.clear();
}
