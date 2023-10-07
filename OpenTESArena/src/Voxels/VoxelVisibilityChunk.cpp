#include "VoxelVisibilityChunk.h"
#include "../Rendering/RenderCamera.h"

namespace
{
	// Child node indices for each internal node (root owns 4 indices, etc.) using breadth-first traversal.
	int CHILD_INDICES[VoxelVisibilityChunk::TOTAL_CHILD_COUNT];
	bool s_isChildIndicesInited = false;

	void InitChildIndexArray()
	{
		DebugAssert(!s_isChildIndicesInited);
		for (int i = 0; i < static_cast<int>(std::size(CHILD_INDICES)); i++)
		{
			CHILD_INDICES[i] = i + 1;
		}
	}

	// Quadtree visibility test state for a node that has to be pushed on the stack so its children nodes can be tested.
	struct SavedSubtreeTestState
	{
		int treeLevelNodeIndex; // 0-# of nodes on this tree level, points to one of the four child nodes.

		SavedSubtreeTestState()
		{
			this->clear();
		}

		void init(int treeLevelNodeIndex)
		{
			this->treeLevelNodeIndex = treeLevelNodeIndex;
		}

		void clear()
		{
			this->treeLevelNodeIndex = -1;
		}
	};

	// Converts a tree level index and tree level node index to a Z-order curve quadkey, where at its most basic is
	// four adjacent nodes in memory arranged in a 2x2 pattern in space.
	int GetZOrderCurveNodeIndex(int treeLevelIndex, int treeLevelNodeIndex)
	{
		DebugAssert(treeLevelIndex < VoxelVisibilityChunk::TREE_LEVEL_COUNT);
		DebugAssert(treeLevelNodeIndex < VoxelVisibilityChunk::LEAF_NODE_COUNT);

		DebugAssertIndex(VoxelVisibilityChunk::NODES_PER_SIDE, treeLevelIndex);
		const int nodesPerSide = VoxelVisibilityChunk::NODES_PER_SIDE[treeLevelIndex];
		const Int2 point = MathUtils::getZOrderCurvePoint(treeLevelNodeIndex);
		return point.x + (point.y * nodesPerSide);
	}

	// Gets the first of four child indices one level down from an internal node.
	int GetFirstChildTreeLevelNodeIndex(int treeLevelNodeIndex)
	{
		return treeLevelNodeIndex * VoxelVisibilityChunk::CHILD_COUNT_PER_NODE;
	}

	// Converts the "0-# of nodes on tree level - 1" value to 0-3 for a specific subtree.
	int GetSubtreeChildNodeIndex(int treeLevelNodeIndex)
	{
		return treeLevelNodeIndex % VoxelVisibilityChunk::CHILD_COUNT_PER_NODE;
	}

	void BroadcastCompleteVisibilityResult(VoxelVisibilityChunk &chunk, int treeLevelIndex, int treeLevelNodeIndex, VisibilityType visibilityType)
	{
		static_assert(VoxelVisibilityChunk::CHILD_COUNT_PER_NODE == 4);
		DebugAssert(treeLevelIndex < VoxelVisibilityChunk::TREE_LEVEL_INDEX_LEAF);
		DebugAssert(visibilityType != VisibilityType::Partial);

		const bool isAtLeastPartiallyVisible = visibilityType != VisibilityType::Outside;

		// Very fast writes if the root node is completely visible/invisible.
		if (treeLevelIndex == VoxelVisibilityChunk::TREE_LEVEL_INDEX_ROOT)
		{
			std::fill(std::begin(chunk.internalNodeVisibilityTypes), std::end(chunk.internalNodeVisibilityTypes), visibilityType);
			std::fill(std::begin(chunk.leafNodeFrustumTests), std::end(chunk.leafNodeFrustumTests), isAtLeastPartiallyVisible);
			return;
		}

		const int firstChildTreeLevelNodeIndex = GetFirstChildTreeLevelNodeIndex(treeLevelNodeIndex);
		int childrenTreeLevelNodeIndices[VoxelVisibilityChunk::CHILD_COUNT_PER_NODE];
		childrenTreeLevelNodeIndices[0] = firstChildTreeLevelNodeIndex;
		childrenTreeLevelNodeIndices[1] = firstChildTreeLevelNodeIndex + 1;
		childrenTreeLevelNodeIndices[2] = firstChildTreeLevelNodeIndex + 2;
		childrenTreeLevelNodeIndices[3] = firstChildTreeLevelNodeIndex + 3;

		const int childrenTreeLevelIndex = treeLevelIndex + 1;
		const bool childrenTreeLevelHasChildNodes = childrenTreeLevelIndex < VoxelVisibilityChunk::TREE_LEVEL_INDEX_LEAF;
		if (childrenTreeLevelHasChildNodes)
		{
			for (const int childTreeLevelNodeIndex : childrenTreeLevelNodeIndices)
			{
				const int zOrderCurveNodeIndex = GetZOrderCurveNodeIndex(childrenTreeLevelIndex, childTreeLevelNodeIndex);
				DebugAssertIndex(chunk.internalNodeVisibilityTypes, zOrderCurveNodeIndex);
				chunk.internalNodeVisibilityTypes[zOrderCurveNodeIndex] = visibilityType;
			}

			// @optimization: do this an iterative way instead
			BroadcastCompleteVisibilityResult(chunk, childrenTreeLevelIndex, firstChildTreeLevelNodeIndex, visibilityType);
			BroadcastCompleteVisibilityResult(chunk, childrenTreeLevelIndex, firstChildTreeLevelNodeIndex + 1, visibilityType);
			BroadcastCompleteVisibilityResult(chunk, childrenTreeLevelIndex, firstChildTreeLevelNodeIndex + 2, visibilityType);
			BroadcastCompleteVisibilityResult(chunk, childrenTreeLevelIndex, firstChildTreeLevelNodeIndex + 3, visibilityType);
		}
		else
		{
			for (const int childTreeLevelNodeIndex : childrenTreeLevelNodeIndices)
			{
				const int zOrderCurveNodeIndex = GetZOrderCurveNodeIndex(childrenTreeLevelIndex, childTreeLevelNodeIndex);
				DebugAssertIndex(chunk.leafNodeFrustumTests, zOrderCurveNodeIndex);
				chunk.leafNodeFrustumTests[zOrderCurveNodeIndex] = isAtLeastPartiallyVisible;
			}
		}
	}
}

VoxelVisibilityChunk::VoxelVisibilityChunk()
{
	if (!s_isChildIndicesInited)
	{
		InitChildIndexArray();
		s_isChildIndicesInited = true;
	}

	std::fill(std::begin(this->internalNodeVisibilityTypes), std::end(this->internalNodeVisibilityTypes), VisibilityType::Outside);
	std::fill(std::begin(this->leafNodeFrustumTests), std::end(this->leafNodeFrustumTests), false);
}

void VoxelVisibilityChunk::init(const ChunkInt2 &position, int height, double ceilingScale)
{
	Chunk::init(position, height);

	const double yMax = static_cast<double>(height) * ceilingScale;
	const CoordDouble3 chunkMinCoord(position, VoxelDouble3::Zero);
	const CoordDouble3 chunkMaxCoord(position, VoxelDouble3(static_cast<SNDouble>(Chunk::WIDTH), yMax, static_cast<WEDouble>(Chunk::DEPTH)));
	const WorldDouble3 chunkMinPoint = VoxelUtils::coordToWorldPoint(chunkMinCoord);
	const WorldDouble3 chunkMaxPoint = VoxelUtils::coordToWorldPoint(chunkMaxCoord);

	// Initialize bounding boxes for each quadtree level.
	for (int i = 0; i < TREE_LEVEL_COUNT; i++)
	{
		// Each level covers the whole chunk but XZ iteration becomes more granular as level count increases.
		const int globalNodeOffset = GLOBAL_NODE_OFFSETS[i];
		const int levelNodeCount = NODE_COUNTS[i];
		const int levelNodesPerSide = NODES_PER_SIDE[i];
		const SNInt xDistPerNode = Chunk::WIDTH / levelNodesPerSide;
		const WEInt zDistPerNode = Chunk::DEPTH / levelNodesPerSide;

		for (WEInt z = 0; z < levelNodesPerSide; z++)
		{
			for (SNInt x = 0; x < levelNodesPerSide; x++)
			{
				const int bboxIndex = globalNodeOffset + (x + (z * levelNodesPerSide));
				DebugAssert(bboxIndex < (globalNodeOffset + levelNodeCount));
				DebugAssertIndex(this->nodeBBoxes, bboxIndex);
				BoundingBox3D &bbox = this->nodeBBoxes[bboxIndex];

				const SNDouble bboxMinX = static_cast<SNDouble>(x * xDistPerNode);
				const SNDouble bboxMaxX = static_cast<SNDouble>((x + 1) * xDistPerNode);
				const double bboxMinY = 0.0;
				const double bboxMaxY = yMax;
				const WEDouble bboxMinZ = static_cast<WEDouble>(z * zDistPerNode);
				const WEDouble bboxMaxZ = static_cast<WEDouble>((z + 1) * zDistPerNode);
				const WorldDouble3 bboxMinPoint = chunkMinPoint + WorldDouble3(bboxMinX, bboxMinY, bboxMinZ);
				const WorldDouble3 bboxMaxPoint = chunkMinPoint + WorldDouble3(bboxMaxX, bboxMaxY, bboxMaxZ);
				bbox.init(bboxMinPoint, bboxMaxPoint);
			}
		}
	}

	std::fill(std::begin(this->internalNodeVisibilityTypes), std::end(this->internalNodeVisibilityTypes), VisibilityType::Outside);
	std::fill(std::begin(this->leafNodeFrustumTests), std::end(this->leafNodeFrustumTests), false);
}

void VoxelVisibilityChunk::update(const RenderCamera &camera)
{
	const WorldDouble3 cameraEye = camera.worldPoint;
	const Double3 frustumNormals[5] =
	{
		camera.forward, camera.leftFrustumNormal, camera.rightFrustumNormal, camera.bottomFrustumNormal, camera.topFrustumNormal
	};

	int currentTreeLevelIndex = 0; // Starts at root, ends at leaves.
	int currentTreeLevelNodeIndex = 0; // 0-# of nodes on the current tree level.
	SavedSubtreeTestState savedSubtreeTestStates[TREE_LEVEL_COUNT - 1];
	int savedSubtreeTestStatesCount = 0;

	do
	{
		const int zOrderCurveNodeIndex = GetZOrderCurveNodeIndex(currentTreeLevelIndex, currentTreeLevelNodeIndex);

		DebugAssertIndex(GLOBAL_NODE_OFFSETS, currentTreeLevelIndex);
		const int bboxIndex = GLOBAL_NODE_OFFSETS[currentTreeLevelIndex] + zOrderCurveNodeIndex;

		DebugAssertIndex(this->nodeBBoxes, bboxIndex);
		const BoundingBox3D &bbox = this->nodeBBoxes[bboxIndex];

		constexpr int bboxCornerCount = 8;
		const WorldDouble3 bboxCorners[bboxCornerCount] =
		{
			bbox.min,
			bbox.min + WorldDouble3(bbox.width, 0.0, 0.0),
			bbox.min + WorldDouble3(0.0, bbox.height, 0.0),
			bbox.min + WorldDouble3(bbox.width, bbox.height, 0.0),
			bbox.min + WorldDouble3(0.0, 0.0, bbox.depth),
			bbox.min + WorldDouble3(bbox.width, 0.0, bbox.depth),
			bbox.min + WorldDouble3(0.0, bbox.height, bbox.depth),
			bbox.max
		};

		bool isBBoxCompletelyVisible = true;
		bool isBBoxCompletelyInvisible = false;
		for (const Double3 &frustumNormal : frustumNormals)
		{
			int insidePoints = 0;
			int outsidePoints = 0;
			for (const WorldDouble3 &cornerPoint : bboxCorners)
			{
				const double dist = MathUtils::distanceToPlane(cornerPoint, cameraEye, frustumNormal);
				if (dist >= 0.0)
				{
					insidePoints++;
				}
				else
				{
					outsidePoints++;
				}
			}

			if (insidePoints < bboxCornerCount)
			{
				isBBoxCompletelyVisible = false;
			}

			if (outsidePoints == bboxCornerCount)
			{
				isBBoxCompletelyInvisible = true;
				break;
			}
		}

		const bool treeLevelHasChildNodes = currentTreeLevelIndex < TREE_LEVEL_INDEX_LEAF;
		if (treeLevelHasChildNodes)
		{
			VisibilityType visibilityType;
			if (isBBoxCompletelyInvisible)
			{
				visibilityType = VisibilityType::Outside;
			}
			else if (isBBoxCompletelyVisible)
			{
				visibilityType = VisibilityType::Inside;
			}
			else
			{
				visibilityType = VisibilityType::Partial;
			}

			DebugAssertIndex(this->internalNodeVisibilityTypes, zOrderCurveNodeIndex);
			this->internalNodeVisibilityTypes[zOrderCurveNodeIndex] = visibilityType;

			if (visibilityType == VisibilityType::Partial)
			{
				const int newSavedSubtreeTestStateIndex = savedSubtreeTestStatesCount;
				DebugAssertIndex(savedSubtreeTestStates, newSavedSubtreeTestStateIndex);
				savedSubtreeTestStates[newSavedSubtreeTestStateIndex].init(currentTreeLevelNodeIndex);
				savedSubtreeTestStatesCount++;
				currentTreeLevelIndex++;
				currentTreeLevelNodeIndex = GetFirstChildTreeLevelNodeIndex(currentTreeLevelNodeIndex);
				continue;
			}
			else
			{
				BroadcastCompleteVisibilityResult(*this, currentTreeLevelIndex, currentTreeLevelNodeIndex, visibilityType);
			}
		}
		else
		{
			DebugAssertIndex(this->leafNodeFrustumTests, zOrderCurveNodeIndex);
			this->leafNodeFrustumTests[zOrderCurveNodeIndex] = !isBBoxCompletelyInvisible;
		}

		// Pop out of saved states, handling the case where it's the last node on a tree level.
		int currentSubtreeChildIndex = GetSubtreeChildNodeIndex(currentTreeLevelNodeIndex);
		while ((currentSubtreeChildIndex == (CHILD_COUNT_PER_NODE - 1)) && (savedSubtreeTestStatesCount > 0))
		{
			const int savedSubtreeTestStateIndex = savedSubtreeTestStatesCount - 1;
			SavedSubtreeTestState &savedSubtreeTestState = savedSubtreeTestStates[savedSubtreeTestStateIndex];
			currentTreeLevelNodeIndex = savedSubtreeTestState.treeLevelNodeIndex;
			currentSubtreeChildIndex = GetSubtreeChildNodeIndex(currentTreeLevelNodeIndex);
			currentTreeLevelIndex--;
			savedSubtreeTestState.clear();
			savedSubtreeTestStatesCount--;
		}

		if (currentTreeLevelIndex == TREE_LEVEL_INDEX_ROOT)
		{
			break;
		}

		currentTreeLevelNodeIndex++;
	} while (GetSubtreeChildNodeIndex(currentTreeLevelNodeIndex) < CHILD_COUNT_PER_NODE);
}

void VoxelVisibilityChunk::clear()
{
	Chunk::clear();
	std::fill(std::begin(this->nodeBBoxes), std::end(this->nodeBBoxes), BoundingBox3D());
	std::fill(std::begin(this->internalNodeVisibilityTypes), std::end(this->internalNodeVisibilityTypes), VisibilityType::Outside);
	std::fill(std::begin(this->leafNodeFrustumTests), std::end(this->leafNodeFrustumTests), false);
}
