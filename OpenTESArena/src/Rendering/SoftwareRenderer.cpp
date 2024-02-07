#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <limits>

#include "ArenaRenderUtils.h"
#include "RenderCamera.h"
#include "RenderDrawCall.h"
#include "RendererUtils.h"
#include "RenderFrameSettings.h"
#include "RenderInitSettings.h"
#include "RenderTransform.h"
#include "SoftwareRenderer.h"
#include "../Assets/TextureBuilder.h"
#include "../Math/Constants.h"
#include "../Math/MathUtils.h"
#include "../Math/Random.h"
#include "../Utilities/Color.h"
#include "../Utilities/Palette.h"
#include "../World/ChunkUtils.h"

#include "components/debug/Debug.h"

namespace swShader
{
	struct PixelShaderPerspectiveCorrection
	{
		double ndcZDepth;
		Double2 texelPercent;
	};

	struct PixelShaderTexture
	{
		const uint8_t *texels;
		int width, height;
		int widthMinusOne, heightMinusOne;
		double widthReal, heightReal;
		TextureSamplingType samplingType;

		void init(const uint8_t *texels, int width, int height, TextureSamplingType samplingType)
		{
			this->texels = texels;
			this->width = width;
			this->height = height;
			this->widthMinusOne = width - 1;
			this->heightMinusOne = height - 1;
			this->widthReal = static_cast<double>(width);
			this->heightReal = static_cast<double>(height);
			this->samplingType = samplingType;
		}
	};

	struct PixelShaderPalette
	{
		const uint32_t *colors;
		int count;
	};

	struct PixelShaderLighting
	{
		const uint8_t *lightTableTexels;
		int lightLevelCount; // # of shades from light to dark.
		double lightLevelCountReal;
		int texelsPerLightLevel; // Should be 256 for 8-bit colors.
		int lightLevel; // The selected row of shades between light and dark.
	};

	struct PixelShaderHorizonMirror
	{
		Double2 horizonScreenSpacePoint; // Based on camera forward direction as XZ vector.
		int reflectedPixelIndex;
		bool isReflectedPixelInFrameBuffer;
		uint8_t fallbackSkyColor;
	};

	struct PixelShaderFrameBuffer
	{
		uint8_t *colors;
		double *depth;
		PixelShaderPalette palette;
		double xPercent, yPercent;
		int pixelIndex;
		bool enableDepthWrite;
	};

	void VertexShader_Basic(const Double3 &vertex, const Matrix4d &mvpMatrix, Double4 &outVertex)
	{
		outVertex = mvpMatrix * Double4(vertex, 1.0);
	}

	void VertexShader_RaisingDoor(const Double3 &vertex, const Double3 &preScaleTranslation, const Matrix4d &translationMatrix, 
		const Matrix4d &rotationMatrix, const Matrix4d &scaleMatrix, const Matrix4d &viewMatrix, const Matrix4d &projectionMatrix,
		Double4 &outVertex)
	{
		// Translate down so floor vertices go underground and ceiling is at y=0.
		const Double4 vertexWithPreScaleTranslation = Double4(vertex + preScaleTranslation, 1.0);

		// Shrink towards y=0 depending on anim percent and door min visible amount.
		const Double4 scaledVertex = scaleMatrix * vertexWithPreScaleTranslation;

		// Translate up to new model space Y position.
		const Double4 resultVertex = scaledVertex - Double4(preScaleTranslation, 0.0);

		outVertex = projectionMatrix * (viewMatrix * (translationMatrix * (rotationMatrix * resultVertex)));
	}

	void VertexShader_Entity(const Double3 &vertex, const Matrix4d &mvpMatrix, Double4 &outVertex)
	{
		outVertex = mvpMatrix * Double4(vertex, 1.0);
	}

	void PixelShader_Opaque(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &texture,
		const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		int texelX = -1;
		int texelY = -1;
		if (texture.samplingType == TextureSamplingType::Default)
		{
			texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
			texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.heightReal), 0, texture.heightMinusOne);
		}
		else if (texture.samplingType == TextureSamplingType::ScreenSpaceRepeatY)
		{
			// @todo chasms: determine how many pixels the original texture should cover, based on what percentage the original texture height is over the original screen height.
			texelX = std::clamp(static_cast<int>(frameBuffer.xPercent * texture.widthReal), 0, texture.widthMinusOne);

			const double v = frameBuffer.yPercent * 2.0;
			const double actualV = v >= 1.0 ? (v - 1.0) : v;
			texelY = std::clamp(static_cast<int>(actualV * texture.heightReal), 0, texture.heightMinusOne);
		}

		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const int shadedTexelIndex = texel + (lighting.lightLevel * lighting.texelsPerLightLevel);
		const uint8_t shadedTexel = lighting.lightTableTexels[shadedTexelIndex];
		frameBuffer.colors[frameBuffer.pixelIndex] = shadedTexel;
		
		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_OpaqueWithAlphaTestLayer(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &opaqueTexture,
		const PixelShaderTexture &alphaTestTexture, const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		const int layerTexelX = std::clamp(static_cast<int>(perspective.texelPercent.x * alphaTestTexture.widthReal), 0, alphaTestTexture.widthMinusOne);
		const int layerTexelY = std::clamp(static_cast<int>(perspective.texelPercent.y * alphaTestTexture.heightReal), 0, alphaTestTexture.heightMinusOne);
		const int layerTexelIndex = layerTexelX + (layerTexelY * alphaTestTexture.width);
		uint8_t texel = alphaTestTexture.texels[layerTexelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			const int texelX = std::clamp(static_cast<int>(frameBuffer.xPercent * opaqueTexture.widthReal), 0, opaqueTexture.widthMinusOne);

			const double v = frameBuffer.yPercent * 2.0;
			const double actualV = v >= 1.0 ? (v - 1.0) : v;
			const int texelY = std::clamp(static_cast<int>(actualV * opaqueTexture.heightReal), 0, opaqueTexture.heightMinusOne);

			const int texelIndex = texelX + (texelY * opaqueTexture.width);
			texel = opaqueTexture.texels[texelIndex];
		}

		const int shadedTexelIndex = texel + (lighting.lightLevel * lighting.texelsPerLightLevel);
		const uint8_t shadedTexel = lighting.lightTableTexels[shadedTexelIndex];
		frameBuffer.colors[frameBuffer.pixelIndex] = shadedTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTested(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &texture,
		const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		const int texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
		const int texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.heightReal), 0, texture.heightMinusOne);
		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		const int shadedTexelIndex = texel + (lighting.lightLevel * lighting.texelsPerLightLevel);
		const uint8_t shadedTexel = lighting.lightTableTexels[shadedTexelIndex];
		frameBuffer.colors[frameBuffer.pixelIndex] = shadedTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTestedWithVariableTexCoordUMin(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &texture,
		double uMin, const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		const double u = std::clamp(uMin + ((1.0 - uMin) * perspective.texelPercent.x), uMin, 1.0);
		const int texelX = std::clamp(static_cast<int>(u * texture.widthReal), 0, texture.widthMinusOne);
		const int texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.height), 0, texture.heightMinusOne);
		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		const int shadedTexelIndex = texel + (lighting.lightLevel * lighting.texelsPerLightLevel);
		const uint8_t shadedTexel = lighting.lightTableTexels[shadedTexelIndex];
		frameBuffer.colors[frameBuffer.pixelIndex] = shadedTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTestedWithVariableTexCoordVMin(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &texture,
		double vMin, const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		const int texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
		const double v = std::clamp(vMin + ((1.0 - vMin) * perspective.texelPercent.y), vMin, 1.0);
		const int texelY = std::clamp(static_cast<int>(v * texture.heightReal), 0, texture.heightMinusOne);

		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		const int shadedTexelIndex = texel + (lighting.lightLevel * lighting.texelsPerLightLevel);
		const uint8_t shadedTexel = lighting.lightTableTexels[shadedTexelIndex];
		frameBuffer.colors[frameBuffer.pixelIndex] = shadedTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTestedWithPaletteIndexLookup(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &texture,
		const PixelShaderTexture &lookupTexture, const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		const int texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
		const int texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.heightReal), 0, texture.heightMinusOne);
		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		const uint8_t replacementTexel = lookupTexture.texels[texel];

		const int shadedTexelIndex = replacementTexel + (lighting.lightLevel * lighting.texelsPerLightLevel);
		const uint8_t shadedTexel = lighting.lightTableTexels[shadedTexelIndex];
		frameBuffer.colors[frameBuffer.pixelIndex] = shadedTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTestedWithLightLevelColor(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &texture,
		const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		const int texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
		const int texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.heightReal), 0, texture.heightMinusOne);
		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		const int lightTableTexelIndex = texel + (lighting.lightLevel * lighting.texelsPerLightLevel);
		const uint8_t resultTexel = lighting.lightTableTexels[lightTableTexelIndex];

		frameBuffer.colors[frameBuffer.pixelIndex] = resultTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTestedWithLightLevelOpacity(const PixelShaderPerspectiveCorrection &perspective, const PixelShaderTexture &texture,
		const PixelShaderLighting &lighting, PixelShaderFrameBuffer &frameBuffer)
	{
		const int texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
		const int texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.heightReal), 0, texture.heightMinusOne);
		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		int lightTableTexelIndex;
		if (ArenaRenderUtils::isLightLevelTexel(texel))
		{
			const int lightLevel = static_cast<int>(texel) - ArenaRenderUtils::PALETTE_INDEX_LIGHT_LEVEL_LOWEST;
			const uint8_t prevFrameBufferPixel = frameBuffer.colors[frameBuffer.pixelIndex];
			lightTableTexelIndex = prevFrameBufferPixel + (lightLevel * lighting.texelsPerLightLevel);
		}
		else
		{
			const int lightTableOffset = lighting.lightLevel * lighting.texelsPerLightLevel;
			if (texel == ArenaRenderUtils::PALETTE_INDEX_LIGHT_LEVEL_SRC1)
			{
				lightTableTexelIndex = lightTableOffset + ArenaRenderUtils::PALETTE_INDEX_LIGHT_LEVEL_DST1;
			}
			else if (texel == ArenaRenderUtils::PALETTE_INDEX_LIGHT_LEVEL_SRC2)
			{
				lightTableTexelIndex = lightTableOffset + ArenaRenderUtils::PALETTE_INDEX_LIGHT_LEVEL_DST2;
			}
			else
			{
				lightTableTexelIndex = lightTableOffset + texel;
			}
		}

		const uint8_t resultTexel = lighting.lightTableTexels[lightTableTexelIndex];
		frameBuffer.colors[frameBuffer.pixelIndex] = resultTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTestedWithPreviousBrightnessLimit(const PixelShaderPerspectiveCorrection &perspective,
		const PixelShaderTexture &texture, PixelShaderFrameBuffer &frameBuffer)
	{
		constexpr int brightnessLimit = 0x3F; // Highest value each RGB component can be.
		constexpr uint8_t brightnessMask = ~brightnessLimit;
		constexpr uint32_t brightnessMaskR = brightnessMask << 16;
		constexpr uint32_t brightnessMaskG = brightnessMask << 8;
		constexpr uint32_t brightnessMaskB = brightnessMask;
		constexpr uint32_t brightnessMaskRGB = brightnessMaskR | brightnessMaskG | brightnessMaskB;

		const uint8_t prevFrameBufferPixel = frameBuffer.colors[frameBuffer.pixelIndex];
		const uint32_t prevFrameBufferColor = frameBuffer.palette.colors[prevFrameBufferPixel];
		const bool isDarkEnough = (prevFrameBufferColor & brightnessMaskRGB) == 0;
		if (!isDarkEnough)
		{
			return;
		}

		const int texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
		const int texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.heightReal), 0, texture.heightMinusOne);
		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		frameBuffer.colors[frameBuffer.pixelIndex] = texel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}

	void PixelShader_AlphaTestedWithHorizonMirror(const PixelShaderPerspectiveCorrection &perspective,
		const PixelShaderTexture &texture, const PixelShaderHorizonMirror &horizon, const PixelShaderLighting &lighting,
		PixelShaderFrameBuffer &frameBuffer)
	{
		const int texelX = std::clamp(static_cast<int>(perspective.texelPercent.x * texture.widthReal), 0, texture.widthMinusOne);
		const int texelY = std::clamp(static_cast<int>(perspective.texelPercent.y * texture.heightReal), 0, texture.heightMinusOne);
		const int texelIndex = texelX + (texelY * texture.width);
		const uint8_t texel = texture.texels[texelIndex];

		const bool isTransparent = texel == ArenaRenderUtils::PALETTE_INDEX_TRANSPARENT;
		if (isTransparent)
		{
			return;
		}

		uint8_t resultTexel;
		const bool isReflective = texel == ArenaRenderUtils::PALETTE_INDEX_PUDDLE_EVEN_ROW;
		if (isReflective)
		{
			if (horizon.isReflectedPixelInFrameBuffer)
			{
				const uint8_t mirroredTexel = frameBuffer.colors[horizon.reflectedPixelIndex];
				resultTexel = mirroredTexel;
			}
			else
			{
				resultTexel = horizon.fallbackSkyColor;
			}
		}
		else
		{
			const int shadedTexelIndex = texel + (lighting.lightLevel * lighting.texelsPerLightLevel);
			resultTexel = lighting.lightTableTexels[shadedTexelIndex];
		}

		frameBuffer.colors[frameBuffer.pixelIndex] = resultTexel;

		if (frameBuffer.enableDepthWrite)
		{
			frameBuffer.depth[frameBuffer.pixelIndex] = perspective.ndcZDepth;
		}
	}
}

// Internal geometry types/functions.
namespace swGeometry
{
	struct TriangleDrawListIndices
	{
		int startIndex;
		int count;

		TriangleDrawListIndices(int startIndex, int count)
		{
			this->startIndex = startIndex;
			this->count = count;
		}
	};

	constexpr int MAX_TRIANGLE_CLIP_RESULTS = 2; // The most triangles generated when clipping a triangle against a clip plane.

	// Caches for triangle clip results.
	// @optimization: make N of these to allow for clipping a different triangle per thread
	Double4 g_clipResultV0s[MAX_TRIANGLE_CLIP_RESULTS];
	Double4 g_clipResultV1s[MAX_TRIANGLE_CLIP_RESULTS];
	Double4 g_clipResultV2s[MAX_TRIANGLE_CLIP_RESULTS];
	Double2 g_clipResultUV0s[MAX_TRIANGLE_CLIP_RESULTS];
	Double2 g_clipResultUV1s[MAX_TRIANGLE_CLIP_RESULTS];
	Double2 g_clipResultUV2s[MAX_TRIANGLE_CLIP_RESULTS];

	// Returns number of triangles the input triangle turned into by being clipped against this clip plane.
	int ProcessClipSpaceTriangle(const Double4 &v0, const Double4 &v1, const Double4 &v2,
		const Double2 &uv0, const Double2 &uv1, const Double2 &uv2, int clipPlaneIndex)
	{
		double v0Diff, v1Diff, v2Diff;
		bool isV0Inside, isV1Inside, isV2Inside;
		switch (clipPlaneIndex)
		{
		case 0:
			v0Diff = v0.x - (-v0.w);
			v1Diff = v1.x - (-v1.w);
			v2Diff = v2.x - (-v2.w);
			isV0Inside = v0Diff >= 0.0;
			isV1Inside = v1Diff >= 0.0;
			isV2Inside = v2Diff >= 0.0;
			break;
		case 1:
			v0Diff = v0.x - v0.w;
			v1Diff = v1.x - v1.w;
			v2Diff = v2.x - v2.w;
			isV0Inside = v0Diff <= 0.0;
			isV1Inside = v1Diff <= 0.0;
			isV2Inside = v2Diff <= 0.0;
			break;
		case 2:
			v0Diff = v0.y - (-v0.w);
			v1Diff = v1.y - (-v1.w);
			v2Diff = v2.y - (-v2.w);
			isV0Inside = v0Diff >= 0.0;
			isV1Inside = v1Diff >= 0.0;
			isV2Inside = v2Diff >= 0.0;
			break;
		case 3:
			v0Diff = v0.y - v0.w;
			v1Diff = v1.y - v1.w;
			v2Diff = v2.y - v2.w;
			isV0Inside = v0Diff <= 0.0;
			isV1Inside = v1Diff <= 0.0;
			isV2Inside = v2Diff <= 0.0;
			break;
		case 4:
			v0Diff = v0.z - (-v0.w);
			v1Diff = v1.z - (-v1.w);
			v2Diff = v2.z - (-v2.w);
			isV0Inside = v0Diff >= 0.0;
			isV1Inside = v1Diff >= 0.0;
			isV2Inside = v2Diff >= 0.0;
			break;
		case 5:
			v0Diff = v0.z - v0.w;
			v1Diff = v1.z - v1.w;
			v2Diff = v2.z - v2.w;
			isV0Inside = v0Diff <= 0.0;
			isV1Inside = v1Diff <= 0.0;
			isV2Inside = v2Diff <= 0.0;
			break;
		default:
			// Invalid clip plane case, don't clip anything.
			return 0;
		}

		// Determine which two line segments are intersecting the clipping plane and generate two new vertices,
		// making sure to keep the original winding order.
		if (isV0Inside)
		{
			if (isV1Inside)
			{
				if (isV2Inside)
				{
					// All vertices visible, no clipping needed.
					g_clipResultV0s[0] = v0;
					g_clipResultV1s[0] = v1;
					g_clipResultV2s[0] = v2;
					g_clipResultUV0s[0] = uv0;
					g_clipResultUV1s[0] = uv1;
					g_clipResultUV2s[0] = uv2;
					return 1;
				}
				else
				{
					// Becomes quad
					// Inside: V0, V1
					// Outside: V2
					const double v1v2PointT = v1Diff / (v1Diff - v2Diff);
					const double v2v0PointT = v2Diff / (v2Diff - v0Diff);
					const Double4 v1v2Point = v1.lerp(v2, v1v2PointT);
					const Double4 v2v0Point = v2.lerp(v0, v2v0PointT);
					const Double2 v1v2PointUV = uv1.lerp(uv2, v1v2PointT);
					const Double2 v2v0PointUV = uv2.lerp(uv0, v2v0PointT);
					g_clipResultV0s[0] = v0;
					g_clipResultV1s[0] = v1;
					g_clipResultV2s[0] = v1v2Point;
					g_clipResultV0s[1] = v1v2Point;
					g_clipResultV1s[1] = v2v0Point;
					g_clipResultV2s[1] = v0;
					g_clipResultUV0s[0] = uv0;
					g_clipResultUV1s[0] = uv1;
					g_clipResultUV2s[0] = v1v2PointUV;
					g_clipResultUV0s[1] = v1v2PointUV;
					g_clipResultUV1s[1] = v2v0PointUV;
					g_clipResultUV2s[1] = uv0;
					return 2;
				}
			}
			else
			{
				if (isV2Inside)
				{
					// Becomes quad
					// Inside: V0, V2
					// Outside: V1
					const double v0v1PointT = v0Diff / (v0Diff - v1Diff);
					const double v1v2PointT = v1Diff / (v1Diff - v2Diff);
					const Double4 v0v1Point = v0.lerp(v1, v0v1PointT);
					const Double4 v1v2Point = v1.lerp(v2, v1v2PointT);
					const Double2 v0v1PointUV = uv0.lerp(uv1, v0v1PointT);
					const Double2 v1v2PointUV = uv1.lerp(uv2, v1v2PointT);
					g_clipResultV0s[0] = v0;
					g_clipResultV1s[0] = v0v1Point;
					g_clipResultV2s[0] = v1v2Point;
					g_clipResultV0s[1] = v1v2Point;
					g_clipResultV1s[1] = v2;
					g_clipResultV2s[1] = v0;
					g_clipResultUV0s[0] = uv0;
					g_clipResultUV1s[0] = v0v1PointUV;
					g_clipResultUV2s[0] = v1v2PointUV;
					g_clipResultUV0s[1] = v1v2PointUV;
					g_clipResultUV1s[1] = uv2;
					g_clipResultUV2s[1] = uv0;
					return 2;
				}
				else
				{
					// Becomes smaller triangle
					// Inside: V0
					// Outside: V1, V2
					const double v0v1PointT = v0Diff / (v0Diff - v1Diff);
					const double v2v0PointT = v2Diff / (v2Diff - v0Diff);
					const Double4 v0v1Point = v0.lerp(v1, v0v1PointT);
					const Double4 v2v0Point = v2.lerp(v0, v2v0PointT);
					const Double2 v0v1PointUV = uv0.lerp(uv1, v0v1PointT);
					const Double2 v2v0PointUV = uv2.lerp(uv0, v2v0PointT);
					g_clipResultV0s[0] = v0;
					g_clipResultV1s[0] = v0v1Point;
					g_clipResultV2s[0] = v2v0Point;
					g_clipResultUV0s[0] = uv0;
					g_clipResultUV1s[0] = v0v1PointUV;
					g_clipResultUV2s[0] = v2v0PointUV;
					return 1;
				}
			}
		}
		else
		{
			if (isV1Inside)
			{
				if (isV2Inside)
				{
					// Becomes quad
					// Inside: V1, V2
					// Outside: V0
					const double v0v1PointT = v0Diff / (v0Diff - v1Diff);
					const double v2v0PointT = v2Diff / (v2Diff - v0Diff);
					const Double4 v0v1Point = v0.lerp(v1, v0v1PointT);
					const Double4 v2v0Point = v2.lerp(v0, v2v0PointT);
					const Double2 v0v1PointUV = uv0.lerp(uv1, v0v1PointT);
					const Double2 v2v0PointUV = uv2.lerp(uv0, v2v0PointT);
					g_clipResultV0s[0] = v0v1Point;
					g_clipResultV1s[0] = v1;
					g_clipResultV2s[0] = v2;
					g_clipResultV0s[1] = v2;
					g_clipResultV1s[1] = v2v0Point;
					g_clipResultV2s[1] = v0v1Point;
					g_clipResultUV0s[0] = v0v1PointUV;
					g_clipResultUV1s[0] = uv1;
					g_clipResultUV2s[0] = uv2;
					g_clipResultUV0s[1] = uv2;
					g_clipResultUV1s[1] = v2v0PointUV;
					g_clipResultUV2s[1] = v0v1PointUV;
					return 2;
				}
				else
				{
					// Becomes smaller triangle
					// Inside: V1
					// Outside: V0, V2
					const double v0v1PointT = v0Diff / (v0Diff - v1Diff);
					const double v1v2PointT = v1Diff / (v1Diff - v2Diff);
					const Double4 v0v1Point = v0.lerp(v1, v0v1PointT);
					const Double4 v1v2Point = v1.lerp(v2, v1v2PointT);
					const Double2 v0v1PointUV = uv0.lerp(uv1, v0v1PointT);
					const Double2 v1v2PointUV = uv1.lerp(uv2, v1v2PointT);
					g_clipResultV0s[0] = v0v1Point;
					g_clipResultV1s[0] = v1;
					g_clipResultV2s[0] = v1v2Point;
					g_clipResultUV0s[0] = v0v1PointUV;
					g_clipResultUV1s[0] = uv1;
					g_clipResultUV2s[0] = v1v2PointUV;
					return 1;
				}
			}
			else
			{
				if (isV2Inside)
				{
					// Becomes smaller triangle
					// Inside: V2
					// Outside: V0, V1
					const double v1v2PointT = v1Diff / (v1Diff - v2Diff);
					const double v2v0PointT = v2Diff / (v2Diff - v0Diff);
					const Double4 v1v2Point = v1.lerp(v2, v1v2PointT);
					const Double4 v2v0Point = v2.lerp(v0, v2v0PointT);
					const Double2 v1v2PointUV = uv1.lerp(uv2, v1v2PointT);
					const Double2 v2v0PointUV = uv2.lerp(uv0, v2v0PointT);
					g_clipResultV0s[0] = v1v2Point;
					g_clipResultV1s[0] = v2;
					g_clipResultV2s[0] = v2v0Point;
					g_clipResultUV0s[0] = v1v2PointUV;
					g_clipResultUV1s[0] = uv2;
					g_clipResultUV2s[0] = v2v0PointUV;
					return 1;
				}
				else
				{
					// All vertices outside frustum.
					return 0;
				}
			}
		}
	}

	constexpr int MAX_CLIPPED_MESH_TRIANGLES = 4096; // The most triangles a processed clip space mesh can have when passed to the rasterizer.
	constexpr int MAX_CLIPPED_TRIANGLE_TRIANGLES = 64; // The most triangles a triangle can generate after being clipped by all clip planes.

	// Triangles generated by clipping the current mesh. These are sent to the rasterizer.
	// @optimization: make N of these to allow for clipping a different mesh per thread
	Double4 g_clipSpaceMeshV0s[MAX_CLIPPED_MESH_TRIANGLES];
	Double4 g_clipSpaceMeshV1s[MAX_CLIPPED_MESH_TRIANGLES];
	Double4 g_clipSpaceMeshV2s[MAX_CLIPPED_MESH_TRIANGLES];
	Double2 g_clipSpaceMeshUV0s[MAX_CLIPPED_MESH_TRIANGLES];
	Double2 g_clipSpaceMeshUV1s[MAX_CLIPPED_MESH_TRIANGLES];
	Double2 g_clipSpaceMeshUV2s[MAX_CLIPPED_MESH_TRIANGLES];
	ObjectTextureID g_clipSpaceMeshTextureID0 = -1;
	ObjectTextureID g_clipSpaceMeshTextureID1 = -1;

	// Triangles generated by clipping the current triangle against clipping planes.
	Double4 g_clipSpaceTriangleV0s[MAX_CLIPPED_TRIANGLE_TRIANGLES];
	Double4 g_clipSpaceTriangleV1s[MAX_CLIPPED_TRIANGLE_TRIANGLES];
	Double4 g_clipSpaceTriangleV2s[MAX_CLIPPED_TRIANGLE_TRIANGLES];
	Double2 g_clipSpaceTriangleUV0s[MAX_CLIPPED_TRIANGLE_TRIANGLES];
	Double2 g_clipSpaceTriangleUV1s[MAX_CLIPPED_TRIANGLE_TRIANGLES];
	Double2 g_clipSpaceTriangleUV2s[MAX_CLIPPED_TRIANGLE_TRIANGLES];

	int g_clipSpaceMeshTriangleCount = 0; // Triangles in the current clip space mesh to be rasterized.
	int g_totalClipSpaceTriangleCount = 0; // All processed triangles in the frustum, including new ones generated by clipping.
	int g_totalDrawCallTriangleCount = 0; // World space triangles generated by iterating index buffers. Doesn't include ones generated by clipping.

	void ClearProcessedTriangles()
	{
		// Skip zeroing mesh arrays for performance.
		g_clipSpaceMeshTextureID0 = -1;
		g_clipSpaceMeshTextureID1 = -1;
		g_clipSpaceMeshTriangleCount = 0;
		g_totalClipSpaceTriangleCount = 0;
		g_totalDrawCallTriangleCount = 0;
	}

	// Forms a world space mesh from the given vertex buffer and index buffer, runs vertex shaders, clips triangles to the frustum,
	// then returns clip space triangle indices for the rasterizer to iterate.
	swGeometry::TriangleDrawListIndices ProcessMeshForRasterization(const Double3 &preScaleTranslation,
		const Matrix4d &translationMatrix, const Matrix4d &rotationMatrix, const Matrix4d &scaleMatrix,
		const Matrix4d &viewMatrix, const Matrix4d &projectionMatrix, const SoftwareRenderer::VertexBuffer &vertexBuffer,
		const SoftwareRenderer::AttributeBuffer &texCoordBuffer, const SoftwareRenderer::IndexBuffer &indexBuffer,
		ObjectTextureID textureID0, ObjectTextureID textureID1, VertexShaderType vertexShaderType)
	{
		// Reset results cache. Skip zeroing the mesh arrays for performance.
		g_clipSpaceMeshTriangleCount = 0;

		// Set rasterizer textures.
		g_clipSpaceMeshTextureID0 = textureID0;
		g_clipSpaceMeshTextureID1 = textureID1;

		const Matrix4d modelMatrix = translationMatrix * (rotationMatrix * scaleMatrix);
		const Matrix4d modelViewProjMatrix = projectionMatrix * (viewMatrix * modelMatrix);

		const double *verticesPtr = vertexBuffer.vertices.begin();
		const double *texCoordsPtr = texCoordBuffer.attributes.begin();
		const int32_t *indicesPtr = indexBuffer.indices.begin();
		const int triangleCount = indexBuffer.indices.getCount() / 3;
		for (int triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++)
		{
			const int indexBufferBase = triangleIndex * 3;
			const int32_t index0 = indicesPtr[indexBufferBase];
			const int32_t index1 = indicesPtr[indexBufferBase + 1];
			const int32_t index2 = indicesPtr[indexBufferBase + 2];
			
			const int32_t v0Index = index0 * 3;
			const int32_t v1Index = index1 * 3;
			const int32_t v2Index = index2 * 3;
			const Double3 unshadedV0(
				*(verticesPtr + v0Index),
				*(verticesPtr + v0Index + 1),
				*(verticesPtr + v0Index + 2));
			const Double3 unshadedV1(
				*(verticesPtr + v1Index),
				*(verticesPtr + v1Index + 1),
				*(verticesPtr + v1Index + 2));
			const Double3 unshadedV2(
				*(verticesPtr + v2Index),
				*(verticesPtr + v2Index + 1),
				*(verticesPtr + v2Index + 2));

			Double4 shadedV0, shadedV1, shadedV2;
			switch (vertexShaderType)
			{
			case VertexShaderType::Basic:
				swShader::VertexShader_Basic(unshadedV0, modelViewProjMatrix, shadedV0);
				swShader::VertexShader_Basic(unshadedV1, modelViewProjMatrix, shadedV1);
				swShader::VertexShader_Basic(unshadedV2, modelViewProjMatrix, shadedV2);
				break;
			case VertexShaderType::RaisingDoor:
				swShader::VertexShader_RaisingDoor(unshadedV0, preScaleTranslation, translationMatrix, rotationMatrix, scaleMatrix, viewMatrix, projectionMatrix, shadedV0);
				swShader::VertexShader_RaisingDoor(unshadedV1, preScaleTranslation, translationMatrix, rotationMatrix, scaleMatrix, viewMatrix, projectionMatrix, shadedV1);
				swShader::VertexShader_RaisingDoor(unshadedV2, preScaleTranslation, translationMatrix, rotationMatrix, scaleMatrix, viewMatrix, projectionMatrix, shadedV2);
				break;
			case VertexShaderType::Entity:
				swShader::VertexShader_Entity(unshadedV0, modelViewProjMatrix, shadedV0);
				swShader::VertexShader_Entity(unshadedV1, modelViewProjMatrix, shadedV1);
				swShader::VertexShader_Entity(unshadedV2, modelViewProjMatrix, shadedV2);
				break;
			default:
				DebugNotImplementedMsg(std::to_string(static_cast<int>(vertexShaderType)));
				break;
			}

			const int32_t uv0Index = index0 * 2;
			const int32_t uv1Index = index1 * 2;
			const int32_t uv2Index = index2 * 2;
			const Double2 uv0(
				*(texCoordsPtr + uv0Index),
				*(texCoordsPtr + uv0Index + 1));
			const Double2 uv1(
				*(texCoordsPtr + uv1Index),
				*(texCoordsPtr + uv1Index + 1));
			const Double2 uv2(
				*(texCoordsPtr + uv2Index),
				*(texCoordsPtr + uv2Index + 1));

			// Initialize clipping loop with the vertex-shaded triangle.
			g_clipSpaceTriangleV0s[0] = shadedV0;
			g_clipSpaceTriangleV1s[0] = shadedV1;
			g_clipSpaceTriangleV2s[0] = shadedV2;
			g_clipSpaceTriangleUV0s[0] = uv0;
			g_clipSpaceTriangleUV1s[0] = uv1;
			g_clipSpaceTriangleUV2s[0] = uv2;

			int clipListSize = 1; // Triangles to process based on this vertex-shaded triangle.
			int clipListFrontIndex = 0;

			constexpr int clipPlaneCount = 6; // Check each dimension against -W and W components.
			for (int clipPlaneIndex = 0; clipPlaneIndex < clipPlaneCount; clipPlaneIndex++)
			{
				const int trianglesToClipCount = clipListSize - clipListFrontIndex;
				for (int triangleToClip = trianglesToClipCount; triangleToClip > 0; triangleToClip--)
				{
					DebugAssert(clipListFrontIndex < MAX_CLIPPED_TRIANGLE_TRIANGLES);
					const Double4 &currentV0 = g_clipSpaceTriangleV0s[clipListFrontIndex];
					const Double4 &currentV1 = g_clipSpaceTriangleV1s[clipListFrontIndex];
					const Double4 &currentV2 = g_clipSpaceTriangleV2s[clipListFrontIndex];
					const Double2 &currentUV0 = g_clipSpaceTriangleUV0s[clipListFrontIndex];
					const Double2 &currentUV1 = g_clipSpaceTriangleUV1s[clipListFrontIndex];
					const Double2 &currentUV2 = g_clipSpaceTriangleUV2s[clipListFrontIndex];
					const int clipResultCount = ProcessClipSpaceTriangle(currentV0, currentV1, currentV2, currentUV0, currentUV1, currentUV2, clipPlaneIndex);

					for (int clipResultIndex = 0; clipResultIndex < clipResultCount; clipResultIndex++)
					{
						const int clipResultWriteIndex = clipListSize + clipResultIndex;
						DebugAssert(clipResultWriteIndex < MAX_CLIPPED_TRIANGLE_TRIANGLES);
						g_clipSpaceTriangleV0s[clipResultWriteIndex] = g_clipResultV0s[clipResultIndex];
						g_clipSpaceTriangleV1s[clipResultWriteIndex] = g_clipResultV1s[clipResultIndex];
						g_clipSpaceTriangleV2s[clipResultWriteIndex] = g_clipResultV2s[clipResultIndex];
						g_clipSpaceTriangleUV0s[clipResultWriteIndex] = g_clipResultUV0s[clipResultIndex];
						g_clipSpaceTriangleUV1s[clipResultWriteIndex] = g_clipResultUV1s[clipResultIndex];
						g_clipSpaceTriangleUV2s[clipResultWriteIndex] = g_clipResultUV2s[clipResultIndex];
					}

					clipListSize += clipResultCount;
					clipListFrontIndex++;
				}
			}

			// Add the clip results to the mesh, skipping the incomplete triangles the front index advanced beyond.
			const int resultTriangleCount = clipListSize - clipListFrontIndex;
			for (int resultTriangleIndex = 0; resultTriangleIndex < resultTriangleCount; resultTriangleIndex++)
			{
				const int srcIndex = clipListFrontIndex + resultTriangleIndex;
				const int dstIndex = g_clipSpaceMeshTriangleCount + resultTriangleIndex;
				g_clipSpaceMeshV0s[dstIndex] = g_clipSpaceTriangleV0s[srcIndex];
				g_clipSpaceMeshV1s[dstIndex] = g_clipSpaceTriangleV1s[srcIndex];
				g_clipSpaceMeshV2s[dstIndex] = g_clipSpaceTriangleV2s[srcIndex];
				g_clipSpaceMeshUV0s[dstIndex] = g_clipSpaceTriangleUV0s[srcIndex];
				g_clipSpaceMeshUV1s[dstIndex] = g_clipSpaceTriangleUV1s[srcIndex];
				g_clipSpaceMeshUV2s[dstIndex] = g_clipSpaceTriangleUV2s[srcIndex];
			}

			g_clipSpaceMeshTriangleCount += resultTriangleCount;
		}
		
		g_totalDrawCallTriangleCount += triangleCount;
		g_totalClipSpaceTriangleCount += g_clipSpaceMeshTriangleCount;
		return swGeometry::TriangleDrawListIndices(0, g_clipSpaceMeshTriangleCount);
	}
}

// Rendering functions, per-pixel work.
namespace swRender
{
	constexpr int DITHERING_MODE_NONE = 0;
	constexpr int DITHERING_MODE_CLASSIC = 1;
	constexpr int DITHERING_MODE_MODERN = 2;

	constexpr int DITHERING_MODE_MODERN_MASK_COUNT = 4;

	int g_totalDrawCallCount = 0;

	// For measuring overdraw.
	int g_totalDepthTests = 0;
	int g_totalColorWrites = 0;

	void CreateDitherBuffer(Buffer3D<bool> &ditherBuffer, int width, int height, int ditheringMode)
	{
		if (ditheringMode == DITHERING_MODE_CLASSIC)
		{
			// Original game: 2x2, top left + bottom right are darkened.
			ditherBuffer.init(width, height, 1);

			bool *ditherPixels = ditherBuffer.begin();
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const bool shouldDither = ((x + y) & 0x1) == 0;
					const int index = x + (y * width);
					ditherPixels[index] = shouldDither;
				}
			}
		}
		else if (ditheringMode == DITHERING_MODE_MODERN)
		{
			// Modern 2x2, four levels of dither depending on percent between two light levels.
			ditherBuffer.init(width, height, DITHERING_MODE_MODERN_MASK_COUNT);
			static_assert(DITHERING_MODE_MODERN_MASK_COUNT == 4);

			bool *ditherPixels = ditherBuffer.begin();
			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					const bool shouldDither0 = (((x + y) & 0x1) == 0) || (((x % 2) == 1) && ((y % 2) == 0)); // Top left, bottom right, top right
					const bool shouldDither1 = ((x + y) & 0x1) == 0; // Top left + bottom right
					const bool shouldDither2 = ((x % 2) == 0) && ((y % 2) == 0); // Top left
					const bool shouldDither3 = false;
					const int index0 = x + (y * width);
					const int index1 = x + (y * width) + (1 * width * height);
					const int index2 = x + (y * width) + (2 * width * height);
					const int index3 = x + (y * width) + (3 * width * height);
					ditherPixels[index0] = shouldDither0;
					ditherPixels[index1] = shouldDither1;
					ditherPixels[index2] = shouldDither2;
					ditherPixels[index3] = shouldDither3;
				}
			}
		}
		else
		{
			ditherBuffer.clear();
		}
	}

	void ClearFrameBuffers(BufferView2D<uint8_t> paletteIndexBuffer, BufferView2D<double> depthBuffer,
		BufferView2D<uint32_t> colorBuffer)
	{
		paletteIndexBuffer.fill(0);
		depthBuffer.fill(std::numeric_limits<double>::infinity());
		colorBuffer.fill(0);
		swRender::g_totalDepthTests = 0;
		swRender::g_totalColorWrites = 0;
	}

	void RasterizeTriangles(const swGeometry::TriangleDrawListIndices &drawListIndices, TextureSamplingType textureSamplingType0,
		TextureSamplingType textureSamplingType1, RenderLightingType lightingType, double meshLightPercent, double ambientPercent,
		BufferView<const SoftwareRenderer::Light*> lights, PixelShaderType pixelShaderType, double pixelShaderParam0,
		bool enableDepthRead, bool enableDepthWrite, const SoftwareRenderer::ObjectTexturePool &textures,
		const SoftwareRenderer::ObjectTexture &paletteTexture, const SoftwareRenderer::ObjectTexture &lightTableTexture,
		const SoftwareRenderer::ObjectTexture &skyBgTexture, int ditheringMode, const RenderCamera &camera,
		BufferView2D<uint8_t> paletteIndexBuffer, BufferView2D<double> depthBuffer, BufferView3D<const bool> ditherBuffer,
		BufferView2D<uint32_t> colorBuffer)
	{
		const int frameBufferWidth = paletteIndexBuffer.getWidth();
		const int frameBufferHeight = paletteIndexBuffer.getHeight();
		const double frameBufferWidthReal = static_cast<double>(frameBufferWidth);
		const double frameBufferHeightReal = static_cast<double>(frameBufferHeight);
		const bool *ditherBufferPtr = ditherBuffer.begin();
		uint32_t *colorBufferPtr = colorBuffer.begin();

		const Matrix4d &viewMatrix = camera.viewMatrix;
		const Matrix4d &projectionMatrix = camera.projectionMatrix;
		const Matrix4d &invViewMatrix = camera.inverseViewMatrix;
		const Matrix4d &invProjMatrix = camera.inverseProjectionMatrix;

		const int lightCount = lights.getCount();
		const SoftwareRenderer::Light **lightsPtr = lights.begin();

		const bool requiresTwoTextures =
			(pixelShaderType == PixelShaderType::OpaqueWithAlphaTestLayer) ||
			(pixelShaderType == PixelShaderType::AlphaTestedWithPaletteIndexLookup);
		const bool requiresHorizonMirror = pixelShaderType == PixelShaderType::AlphaTestedWithHorizonMirror;
		const bool requiresPerPixelLightIntensity = lightingType == RenderLightingType::PerPixel;
		const bool requiresPerMeshLightIntensity = lightingType == RenderLightingType::PerMesh;

		swShader::PixelShaderLighting shaderLighting;
		shaderLighting.lightTableTexels = lightTableTexture.texels8Bit;
		shaderLighting.lightLevelCount = lightTableTexture.height;
		shaderLighting.lightLevelCountReal = static_cast<double>(shaderLighting.lightLevelCount);
		shaderLighting.texelsPerLightLevel = lightTableTexture.width;
		shaderLighting.lightLevel = 0;

		swShader::PixelShaderFrameBuffer shaderFrameBuffer;
		shaderFrameBuffer.colors = paletteIndexBuffer.begin();
		shaderFrameBuffer.depth = depthBuffer.begin();
		shaderFrameBuffer.palette.colors = paletteTexture.texels32Bit;
		shaderFrameBuffer.palette.count = paletteTexture.texelCount;
		shaderFrameBuffer.enableDepthWrite = enableDepthWrite;

		swShader::PixelShaderHorizonMirror shaderHorizonMirror;
		if (requiresHorizonMirror)
		{
			// @todo: this doesn't support roll. will need something like a vector projection later.
			const Double3 horizonWorldPoint = camera.worldPoint + camera.horizonDir;
			const Double4 horizonCameraPoint = RendererUtils::worldSpaceToCameraSpace(Double4(horizonWorldPoint, 1.0), viewMatrix);
			const Double4 horizonClipPoint = RendererUtils::cameraSpaceToClipSpace(horizonCameraPoint, projectionMatrix);
			const Double3 horizonNdcPoint = RendererUtils::clipSpaceToNDC(horizonClipPoint);
			const Double2 horizonScreenSpacePoint = RendererUtils::ndcToScreenSpace(horizonNdcPoint, frameBufferWidthReal, frameBufferHeightReal);
			shaderHorizonMirror.horizonScreenSpacePoint = horizonScreenSpacePoint;

			DebugAssert(skyBgTexture.texelCount > 0);
			shaderHorizonMirror.fallbackSkyColor = skyBgTexture.texels8Bit[0];
		}

		const int triangleCount = drawListIndices.count;
		for (int i = 0; i < triangleCount; i++)
		{
			const int index = drawListIndices.startIndex + i;
			const Double4 &clip0 = swGeometry::g_clipSpaceMeshV0s[index];
			const Double4 &clip1 = swGeometry::g_clipSpaceMeshV1s[index];
			const Double4 &clip2 = swGeometry::g_clipSpaceMeshV2s[index];
			const Double3 ndc0 = RendererUtils::clipSpaceToNDC(clip0);
			const Double3 ndc1 = RendererUtils::clipSpaceToNDC(clip1);
			const Double3 ndc2 = RendererUtils::clipSpaceToNDC(clip2);
			const Double2 screenSpace0 = RendererUtils::ndcToScreenSpace(ndc0, frameBufferWidthReal, frameBufferHeightReal);
			const Double2 screenSpace1 = RendererUtils::ndcToScreenSpace(ndc1, frameBufferWidthReal, frameBufferHeightReal);
			const Double2 screenSpace2 = RendererUtils::ndcToScreenSpace(ndc2, frameBufferWidthReal, frameBufferHeightReal);
			const Double2 screenSpace01 = screenSpace1 - screenSpace0;
			const Double2 screenSpace12 = screenSpace2 - screenSpace1;
			const Double2 screenSpace20 = screenSpace0 - screenSpace2;
			const double screenSpace01Cross12 = screenSpace12.cross(screenSpace01);
			const double screenSpace12Cross20 = screenSpace20.cross(screenSpace12);
			const double screenSpace20Cross01 = screenSpace01.cross(screenSpace20);

			// Discard back-facing.
			const bool isFrontFacing = (screenSpace01Cross12 + screenSpace12Cross20 + screenSpace20Cross01) > 0.0;
			if (!isFrontFacing)
			{
				continue;
			}

			const Double2 screenSpace01Perp = screenSpace01.rightPerp();
			const Double2 screenSpace12Perp = screenSpace12.rightPerp();
			const Double2 screenSpace20Perp = screenSpace20.rightPerp();

			// Naive screen-space bounding box around triangle.
			const double xMin = std::min(screenSpace0.x, std::min(screenSpace1.x, screenSpace2.x));
			const double xMax = std::max(screenSpace0.x, std::max(screenSpace1.x, screenSpace2.x));
			const double yMin = std::min(screenSpace0.y, std::min(screenSpace1.y, screenSpace2.y));
			const double yMax = std::max(screenSpace0.y, std::max(screenSpace1.y, screenSpace2.y));
			const int xStart = RendererUtils::getLowerBoundedPixel(xMin, frameBufferWidth);
			const int xEnd = RendererUtils::getUpperBoundedPixel(xMax, frameBufferWidth);
			const int yStart = RendererUtils::getLowerBoundedPixel(yMin, frameBufferHeight);
			const int yEnd = RendererUtils::getUpperBoundedPixel(yMax, frameBufferHeight);

			const Double2 &uv0 = swGeometry::g_clipSpaceMeshUV0s[index];
			const Double2 &uv1 = swGeometry::g_clipSpaceMeshUV1s[index];
			const Double2 &uv2 = swGeometry::g_clipSpaceMeshUV2s[index];

			const ObjectTextureID textureID0 = swGeometry::g_clipSpaceMeshTextureID0;
			const ObjectTextureID textureID1 = swGeometry::g_clipSpaceMeshTextureID1;
			const SoftwareRenderer::ObjectTexture &texture0 = textures.get(textureID0);

			swShader::PixelShaderTexture shaderTexture0;
			shaderTexture0.init(texture0.texels8Bit, texture0.width, texture0.height, textureSamplingType0);

			swShader::PixelShaderTexture shaderTexture1;
			if (requiresTwoTextures)
			{
				const SoftwareRenderer::ObjectTexture &texture1 = textures.get(textureID1);
				shaderTexture1.init(texture1.texels8Bit, texture1.width, texture1.height, textureSamplingType1);
			}

			for (int y = yStart; y < yEnd; y++)
			{
				shaderFrameBuffer.yPercent = (static_cast<double>(y) + 0.50) / frameBufferHeightReal;

				for (int x = xStart; x < xEnd; x++)
				{
					shaderFrameBuffer.xPercent = (static_cast<double>(x) + 0.50) / frameBufferWidthReal;
					const Double2 pixelCenter(
						shaderFrameBuffer.xPercent * frameBufferWidthReal,
						shaderFrameBuffer.yPercent * frameBufferHeightReal);

					// See if pixel center is inside triangle.
					const bool inHalfSpace0 = MathUtils::isPointInHalfSpace(pixelCenter, screenSpace0, screenSpace01Perp);
					const bool inHalfSpace1 = MathUtils::isPointInHalfSpace(pixelCenter, screenSpace1, screenSpace12Perp);
					const bool inHalfSpace2 = MathUtils::isPointInHalfSpace(pixelCenter, screenSpace2, screenSpace20Perp);
					if (inHalfSpace0 && inHalfSpace1 && inHalfSpace2)
					{
						const Double2 &ss0 = screenSpace01;
						const Double2 ss1 = screenSpace2 - screenSpace0;
						const Double2 ss2 = pixelCenter - screenSpace0;

						const double dot00 = ss0.dot(ss0);
						const double dot01 = ss0.dot(ss1);
						const double dot11 = ss1.dot(ss1);
						const double dot20 = ss2.dot(ss0);
						const double dot21 = ss2.dot(ss1);
						const double denominator = (dot00 * dot11) - (dot01 * dot01);

						const double v = ((dot11 * dot20) - (dot01 * dot21)) / denominator;
						const double w = ((dot00 * dot21) - (dot01 * dot20)) / denominator;
						const double u = 1.0 - v - w;

						swShader::PixelShaderPerspectiveCorrection shaderPerspective;
						shaderPerspective.ndcZDepth = (ndc0.z * u) + (ndc1.z * v) + (ndc2.z * w);

						shaderFrameBuffer.pixelIndex = x + (y * frameBufferWidth);
						if (enableDepthRead)
						{
							g_totalDepthTests++;
						}

						if (!enableDepthRead || (shaderPerspective.ndcZDepth < shaderFrameBuffer.depth[shaderFrameBuffer.pixelIndex]))
						{
							const Double4 shaderClipSpacePoint(
								((clip0.x / clip0.w) * u) + ((clip1.x / clip1.w) * v) + ((clip2.x / clip2.w) * w),
								((clip0.y / clip0.w) * u) + ((clip1.y / clip1.w) * v) + ((clip2.y / clip2.w) * w),
								((clip0.z / clip0.w) * u) + ((clip1.z / clip1.w) * v) + ((clip2.z / clip2.w) * w),
								((1.0 / clip0.w) * u) + ((1.0 / clip1.w) * v) + ((1.0 / clip2.w) * w));
							
							shaderPerspective.texelPercent.x = (((uv0.x / clip0.w) * u) + ((uv1.x / clip1.w) * v) + ((uv2.x / clip2.w) * w)) / shaderClipSpacePoint.w;
							shaderPerspective.texelPercent.y = (((uv0.y / clip0.w) * u) + ((uv1.y / clip1.w) * v) + ((uv2.y / clip2.w) * w)) / shaderClipSpacePoint.w;

							const Double4 shaderHomogeneousSpacePoint(
								shaderClipSpacePoint.x / shaderClipSpacePoint.w,
								shaderClipSpacePoint.y / shaderClipSpacePoint.w,
								shaderClipSpacePoint.z / shaderClipSpacePoint.w,
								1.0 / shaderClipSpacePoint.w);
							const Double4 shaderCameraSpacePoint = invProjMatrix * shaderHomogeneousSpacePoint;
							const Double4 shaderWorldSpacePoint = invViewMatrix * shaderCameraSpacePoint;
							const Double3 shaderWorldSpacePointXYZ(shaderWorldSpacePoint.x, shaderWorldSpacePoint.y, shaderWorldSpacePoint.z);

							double lightIntensitySum = 0.0;
							if (requiresPerPixelLightIntensity)
							{
								lightIntensitySum = ambientPercent;
								for (int lightIndex = 0; lightIndex < lightCount; lightIndex++)
								{
									const SoftwareRenderer::Light &light = *lightsPtr[lightIndex];
									const Double3 lightPointDiff = light.worldPoint - shaderWorldSpacePointXYZ;
									const double lightDistance = lightPointDiff.length();
									double lightIntensity;
									if (lightDistance <= light.startRadius)
									{
										lightIntensity = 1.0;
									}
									else if (lightDistance >= light.endRadius)
									{
										lightIntensity = 0.0;
									}
									else
									{
										lightIntensity = std::clamp(1.0 - ((lightDistance - light.startRadius) / (light.endRadius - light.startRadius)), 0.0, 1.0);
									}

									lightIntensitySum += lightIntensity;

									if (lightIntensitySum >= 1.0)
									{
										lightIntensitySum = 1.0;
										break;
									}
								}
							}
							else if (requiresPerMeshLightIntensity)
							{
								lightIntensitySum = meshLightPercent;
							}

							const double lightLevelReal = lightIntensitySum * shaderLighting.lightLevelCountReal;
							shaderLighting.lightLevel = (shaderLighting.lightLevelCount - 1) - std::clamp(static_cast<int>(lightLevelReal), 0, shaderLighting.lightLevelCount - 1);

							if (requiresPerPixelLightIntensity)
							{
								// Dither the light level in screen space.
								bool shouldDither;
								switch (ditheringMode)
								{
								case DITHERING_MODE_NONE:
									shouldDither = false;
									break;
								case DITHERING_MODE_CLASSIC:
									shouldDither = ditherBufferPtr[x + (y * frameBufferWidth)];
									break;
								case DITHERING_MODE_MODERN:
									if (lightIntensitySum < 1.0) // Keeps from dithering right next to the camera, not sure why the lowest dither level doesn't do this.
									{
										constexpr int maskCount = DITHERING_MODE_MODERN_MASK_COUNT;
										const double lightLevelFraction = lightLevelReal - std::floor(lightLevelReal);
										const int maskIndex = std::clamp(static_cast<int>(static_cast<double>(maskCount) * lightLevelFraction), 0, maskCount - 1);
										const int ditherBufferIndex = x + (y * frameBufferWidth) + (maskIndex * frameBufferWidth * frameBufferHeight);
										shouldDither = ditherBufferPtr[ditherBufferIndex];
									}
									else
									{
										shouldDither = false;
									}
									break;
								default:
									shouldDither = false;
									break;
								}

								if (shouldDither)
								{
									shaderLighting.lightLevel = std::min(shaderLighting.lightLevel + 1, shaderLighting.lightLevelCount - 1);
								}
							}

							if (requiresHorizonMirror)
							{
								// @todo: support camera roll
								const Double2 reflectedScreenSpacePoint(
									pixelCenter.x,
									shaderHorizonMirror.horizonScreenSpacePoint.y + (shaderHorizonMirror.horizonScreenSpacePoint.y - pixelCenter.y));

								const int reflectedPixelX = static_cast<int>(reflectedScreenSpacePoint.x);
								const int reflectedPixelY = static_cast<int>(reflectedScreenSpacePoint.y);
								shaderHorizonMirror.isReflectedPixelInFrameBuffer =
									(reflectedPixelX >= 0) && (reflectedPixelX < frameBufferWidth) &&
									(reflectedPixelY >= 0) && (reflectedPixelY < frameBufferHeight);
								shaderHorizonMirror.reflectedPixelIndex = reflectedPixelX + (reflectedPixelY * frameBufferWidth);
							}

							switch (pixelShaderType)
							{
							case PixelShaderType::Opaque:
								swShader::PixelShader_Opaque(shaderPerspective, shaderTexture0, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::OpaqueWithAlphaTestLayer:
								swShader::PixelShader_OpaqueWithAlphaTestLayer(shaderPerspective, shaderTexture0, shaderTexture1, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTested:
								swShader::PixelShader_AlphaTested(shaderPerspective, shaderTexture0, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTestedWithVariableTexCoordUMin:
								swShader::PixelShader_AlphaTestedWithVariableTexCoordUMin(shaderPerspective, shaderTexture0, pixelShaderParam0, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTestedWithVariableTexCoordVMin:
								swShader::PixelShader_AlphaTestedWithVariableTexCoordVMin(shaderPerspective, shaderTexture0, pixelShaderParam0, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTestedWithPaletteIndexLookup:
								swShader::PixelShader_AlphaTestedWithPaletteIndexLookup(shaderPerspective, shaderTexture0, shaderTexture1, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTestedWithLightLevelColor:
								swShader::PixelShader_AlphaTestedWithLightLevelColor(shaderPerspective, shaderTexture0, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTestedWithLightLevelOpacity:
								swShader::PixelShader_AlphaTestedWithLightLevelOpacity(shaderPerspective, shaderTexture0, shaderLighting, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTestedWithPreviousBrightnessLimit:
								swShader::PixelShader_AlphaTestedWithPreviousBrightnessLimit(shaderPerspective, shaderTexture0, shaderFrameBuffer);
								break;
							case PixelShaderType::AlphaTestedWithHorizonMirror:
								swShader::PixelShader_AlphaTestedWithHorizonMirror(shaderPerspective, shaderTexture0, shaderHorizonMirror, shaderLighting, shaderFrameBuffer);
								break;
							default:
								DebugNotImplementedMsg(std::to_string(static_cast<int>(pixelShaderType)));
								break;
							}

							// Write pixel shader result to final output buffer. This only results in overdraw for ghosts.
							const uint8_t writtenPaletteIndex = shaderFrameBuffer.colors[shaderFrameBuffer.pixelIndex];
							colorBufferPtr[shaderFrameBuffer.pixelIndex] = shaderFrameBuffer.palette.colors[writtenPaletteIndex];
							g_totalColorWrites++;
						}
					}
				}
			}
		}
	}
}

SoftwareRenderer::ObjectTexture::ObjectTexture()
{
	this->texels8Bit = nullptr;
	this->texels32Bit = nullptr;
	this->width = 0;
	this->height = 0;
	this->widthReal = 0.0;
	this->heightReal = 0.0;
	this->texelCount = 0;
	this->bytesPerTexel = 0;
}

void SoftwareRenderer::ObjectTexture::init(int width, int height, int bytesPerTexel)
{
	DebugAssert(width > 0);
	DebugAssert(height > 0);
	DebugAssert(bytesPerTexel > 0);

	this->texelCount = width * height;
	this->texels.init(this->texelCount * bytesPerTexel);
	this->texels.fill(static_cast<std::byte>(0));

	switch (bytesPerTexel)
	{
	case 1:
		this->texels8Bit = reinterpret_cast<const uint8_t*>(this->texels.begin());
		break;
	case 4:
		this->texels32Bit = reinterpret_cast<const uint32_t*>(this->texels.begin());
		break;
	default:
		DebugNotImplementedMsg(std::to_string(bytesPerTexel));
		break;
	}

	this->width = width;
	this->height = height;
	this->widthReal = static_cast<double>(width);
	this->heightReal = static_cast<double>(height);
	this->bytesPerTexel = bytesPerTexel;
}

void SoftwareRenderer::ObjectTexture::clear()
{
	this->texels.clear();
}

void SoftwareRenderer::VertexBuffer::init(int vertexCount, int componentsPerVertex)
{
	const int valueCount = vertexCount * componentsPerVertex;
	this->vertices.init(valueCount);
}

void SoftwareRenderer::AttributeBuffer::init(int vertexCount, int componentsPerVertex)
{
	const int valueCount = vertexCount * componentsPerVertex;
	this->attributes.init(valueCount);
}

void SoftwareRenderer::IndexBuffer::init(int indexCount)
{
	this->indices.init(indexCount);
}

SoftwareRenderer::Light::Light()
{
	this->startRadius = 0.0;
	this->endRadius = 0.0;
}

void SoftwareRenderer::Light::init(const Double3 &worldPoint, double startRadius, double endRadius)
{
	this->worldPoint = worldPoint;
	this->startRadius = startRadius;
	this->endRadius = endRadius;
}

SoftwareRenderer::SoftwareRenderer()
{
	this->ditheringMode = -1;
}

SoftwareRenderer::~SoftwareRenderer()
{

}

void SoftwareRenderer::init(const RenderInitSettings &settings)
{
	this->paletteIndexBuffer.init(settings.width, settings.height);
	this->depthBuffer.init(settings.width, settings.height);
	
	swRender::CreateDitherBuffer(this->ditherBuffer, settings.width, settings.height, settings.ditheringMode);
	this->ditheringMode = settings.ditheringMode;
}

void SoftwareRenderer::shutdown()
{
	this->paletteIndexBuffer.clear();
	this->depthBuffer.clear();
	this->ditherBuffer.clear();
	this->ditheringMode = -1;
	this->vertexBuffers.clear();
	this->attributeBuffers.clear();
	this->indexBuffers.clear();
	this->uniformBuffers.clear();
	this->objectTextures.clear();
	this->lights.clear();
}

bool SoftwareRenderer::isInited() const
{
	return true;
}

void SoftwareRenderer::resize(int width, int height)
{
	this->paletteIndexBuffer.init(width, height);
	this->paletteIndexBuffer.fill(0);

	this->depthBuffer.init(width, height);
	this->depthBuffer.fill(std::numeric_limits<double>::infinity());

	swRender::CreateDitherBuffer(this->ditherBuffer, width, height, this->ditheringMode);
}

bool SoftwareRenderer::tryCreateVertexBuffer(int vertexCount, int componentsPerVertex, VertexBufferID *outID)
{
	DebugAssert(vertexCount > 0);
	DebugAssert(componentsPerVertex >= 2);

	if (!this->vertexBuffers.tryAlloc(outID))
	{
		DebugLogError("Couldn't allocate vertex buffer ID.");
		return false;
	}

	VertexBuffer &buffer = this->vertexBuffers.get(*outID);
	buffer.init(vertexCount, componentsPerVertex);
	return true;
}

bool SoftwareRenderer::tryCreateAttributeBuffer(int vertexCount, int componentsPerVertex, AttributeBufferID *outID)
{
	DebugAssert(vertexCount > 0);
	DebugAssert(componentsPerVertex >= 2);

	if (!this->attributeBuffers.tryAlloc(outID))
	{
		DebugLogError("Couldn't allocate attribute buffer ID.");
		return false;
	}

	AttributeBuffer &buffer = this->attributeBuffers.get(*outID);
	buffer.init(vertexCount, componentsPerVertex);
	return true;
}

bool SoftwareRenderer::tryCreateIndexBuffer(int indexCount, IndexBufferID *outID)
{
	DebugAssert(indexCount > 0);
	DebugAssert((indexCount % 3) == 0);

	if (!this->indexBuffers.tryAlloc(outID))
	{
		DebugLogError("Couldn't allocate index buffer ID.");
		return false;
	}

	IndexBuffer &buffer = this->indexBuffers.get(*outID);
	buffer.init(indexCount);
	return true;
}

void SoftwareRenderer::populateVertexBuffer(VertexBufferID id, BufferView<const double> vertices)
{
	VertexBuffer &buffer = this->vertexBuffers.get(id);
	const int srcCount = vertices.getCount();
	const int dstCount = buffer.vertices.getCount();
	if (srcCount != dstCount)
	{
		DebugLogError("Mismatched vertex buffer sizes for ID " + std::to_string(id) + ": " +
			std::to_string(srcCount) + " != " + std::to_string(dstCount));
		return;
	}

	const auto srcBegin = vertices.begin();
	const auto srcEnd = srcBegin + srcCount;
	std::copy(srcBegin, srcEnd, buffer.vertices.begin());
}

void SoftwareRenderer::populateAttributeBuffer(AttributeBufferID id, BufferView<const double> attributes)
{
	AttributeBuffer &buffer = this->attributeBuffers.get(id);
	const int srcCount = attributes.getCount();
	const int dstCount = buffer.attributes.getCount();
	if (srcCount != dstCount)
	{
		DebugLogError("Mismatched attribute buffer sizes for ID " + std::to_string(id) + ": " +
			std::to_string(srcCount) + " != " + std::to_string(dstCount));
		return;
	}

	const auto srcBegin = attributes.begin();
	const auto srcEnd = srcBegin + srcCount;
	std::copy(srcBegin, srcEnd, buffer.attributes.begin());
}

void SoftwareRenderer::populateIndexBuffer(IndexBufferID id, BufferView<const int32_t> indices)
{
	IndexBuffer &buffer = this->indexBuffers.get(id);
	const int srcCount = indices.getCount();
	const int dstCount = buffer.indices.getCount();
	if (srcCount != dstCount)
	{
		DebugLogError("Mismatched index buffer sizes for ID " + std::to_string(id) + ": " +
			std::to_string(srcCount) + " != " + std::to_string(dstCount));
		return;
	}

	const auto srcBegin = indices.begin();
	const auto srcEnd = srcBegin + srcCount;
	std::copy(srcBegin, srcEnd, buffer.indices.begin());
}

void SoftwareRenderer::freeVertexBuffer(VertexBufferID id)
{
	this->vertexBuffers.free(id);
}

void SoftwareRenderer::freeAttributeBuffer(AttributeBufferID id)
{
	this->attributeBuffers.free(id);
}

void SoftwareRenderer::freeIndexBuffer(IndexBufferID id)
{
	this->indexBuffers.free(id);
}

bool SoftwareRenderer::tryCreateObjectTexture(int width, int height, int bytesPerTexel, ObjectTextureID *outID)
{
	if (!this->objectTextures.tryAlloc(outID))
	{
		DebugLogError("Couldn't allocate object texture ID.");
		return false;
	}

	ObjectTexture &texture = this->objectTextures.get(*outID);
	texture.init(width, height, bytesPerTexel);
	return true;
}

bool SoftwareRenderer::tryCreateObjectTexture(const TextureBuilder &textureBuilder, ObjectTextureID *outID)
{
	const int width = textureBuilder.getWidth();
	const int height = textureBuilder.getHeight();
	const int bytesPerTexel = textureBuilder.getBytesPerTexel();
	if (!this->tryCreateObjectTexture(width, height, bytesPerTexel, outID))
	{
		DebugLogWarning("Couldn't create " + std::to_string(width) + "x" + std::to_string(height) + " object texture.");
		return false;
	}

	const TextureBuilderType textureBuilderType = textureBuilder.getType();
	ObjectTexture &texture = this->objectTextures.get(*outID);
	if (textureBuilderType == TextureBuilderType::Paletted)
	{
		const TextureBuilder::PalettedTexture &palettedTexture = textureBuilder.getPaletted();
		const Buffer2D<uint8_t> &srcTexels = palettedTexture.texels;
		uint8_t *dstTexels = reinterpret_cast<uint8_t*>(texture.texels.begin());
		std::copy(srcTexels.begin(), srcTexels.end(), dstTexels);
	}
	else if (textureBuilderType == TextureBuilderType::TrueColor)
	{
		const TextureBuilder::TrueColorTexture &trueColorTexture = textureBuilder.getTrueColor();
		const Buffer2D<uint32_t> &srcTexels = trueColorTexture.texels;
		uint32_t *dstTexels = reinterpret_cast<uint32_t*>(texture.texels.begin());
		std::copy(srcTexels.begin(), srcTexels.end(), dstTexels);
	}
	else
	{
		DebugUnhandledReturnMsg(bool, std::to_string(static_cast<int>(textureBuilderType)));
	}

	return true;
}

LockedTexture SoftwareRenderer::lockObjectTexture(ObjectTextureID id)
{
	ObjectTexture &texture = this->objectTextures.get(id);
	return LockedTexture(texture.texels.begin(), texture.bytesPerTexel);
}

void SoftwareRenderer::unlockObjectTexture(ObjectTextureID id)
{
	// Do nothing; any writes are already in RAM.
	static_cast<void>(id);
}

void SoftwareRenderer::freeObjectTexture(ObjectTextureID id)
{
	this->objectTextures.free(id);
}

std::optional<Int2> SoftwareRenderer::tryGetObjectTextureDims(ObjectTextureID id) const
{
	const ObjectTexture &texture = this->objectTextures.get(id);
	return Int2(texture.width, texture.height);
}

bool SoftwareRenderer::tryCreateUniformBuffer(int elementCount, size_t sizeOfElement, size_t alignmentOfElement, UniformBufferID *outID)
{
	DebugAssert(elementCount >= 0);
	DebugAssert(sizeOfElement > 0);
	DebugAssert(alignmentOfElement > 0);

	if (!this->uniformBuffers.tryAlloc(outID))
	{
		DebugLogError("Couldn't allocate uniform buffer ID.");
		return false;
	}

	UniformBuffer &buffer = this->uniformBuffers.get(*outID);
	buffer.init(elementCount, sizeOfElement, alignmentOfElement);
	return true;
}

void SoftwareRenderer::populateUniformBuffer(UniformBufferID id, BufferView<const std::byte> data)
{
	UniformBuffer &buffer = this->uniformBuffers.get(id);
	const int srcCount = data.getCount();
	const int dstCount = buffer.getValidByteCount();
	if (srcCount != dstCount)
	{
		DebugLogError("Mismatched uniform buffer sizes for ID " + std::to_string(id) + ": " +
			std::to_string(srcCount) + " != " + std::to_string(dstCount));
		return;
	}

	const std::byte *srcBegin = data.begin();
	const std::byte *srcEnd = srcBegin + srcCount;
	std::copy(srcBegin, srcEnd, buffer.begin());
}

void SoftwareRenderer::populateUniformAtIndex(UniformBufferID id, int uniformIndex, BufferView<const std::byte> uniformData)
{
	UniformBuffer &buffer = this->uniformBuffers.get(id);
	const int srcByteCount = uniformData.getCount();
	const int dstByteCount = static_cast<int>(buffer.sizeOfElement);
	if (srcByteCount != dstByteCount)
	{
		DebugLogError("Mismatched uniform size for uniform buffer ID " + std::to_string(id) + " index " +
			std::to_string(uniformIndex) + ": " + std::to_string(srcByteCount) + " != " + std::to_string(dstByteCount));
		return;
	}

	const std::byte *srcBegin = uniformData.begin();
	const std::byte *srcEnd = srcBegin + srcByteCount;
	std::byte *dstBegin = buffer.begin() + (dstByteCount * uniformIndex);
	std::copy(srcBegin, srcEnd, dstBegin);
}

void SoftwareRenderer::freeUniformBuffer(UniformBufferID id)
{
	this->uniformBuffers.free(id);
}

bool SoftwareRenderer::tryCreateLight(RenderLightID *outID)
{
	if (!this->lights.tryAlloc(outID))
	{
		DebugLogError("Couldn't allocate render light ID.");
		return false;
	}

	return true;
}

void SoftwareRenderer::setLightPosition(RenderLightID id, const Double3 &worldPoint)
{
	Light &light = this->lights.get(id);
	light.worldPoint = worldPoint;
}

void SoftwareRenderer::setLightRadius(RenderLightID id, double startRadius, double endRadius)
{
	DebugAssert(startRadius >= 0.0);
	DebugAssert(endRadius >= startRadius);
	Light &light = this->lights.get(id);
	light.startRadius = startRadius;
	light.endRadius = endRadius;
}

void SoftwareRenderer::freeLight(RenderLightID id)
{
	this->lights.free(id);
}

RendererSystem3D::ProfilerData SoftwareRenderer::getProfilerData() const
{
	const int renderWidth = this->paletteIndexBuffer.getWidth();
	const int renderHeight = this->paletteIndexBuffer.getHeight();

	const int threadCount = 1;

	const int drawCallCount = swRender::g_totalDrawCallCount;
	const int sceneTriangleCount = swGeometry::g_totalDrawCallTriangleCount;
	const int visTriangleCount = swGeometry::g_totalClipSpaceTriangleCount;

	const int textureCount = this->objectTextures.getUsedCount();
	int textureByteCount = 0;
	for (int i = 0; i < this->objectTextures.getTotalCount(); i++)
	{
		const ObjectTextureID id = static_cast<ObjectTextureID>(i);
		const ObjectTexture *texturePtr = this->objectTextures.tryGet(id);
		if (texturePtr != nullptr)
		{
			textureByteCount += texturePtr->texels.getCount();
		}
	}

	const int totalLightCount = this->lights.getUsedCount();
	const int totalDepthTests = swRender::g_totalDepthTests;
	const int totalColorWrites = swRender::g_totalColorWrites;

	return ProfilerData(renderWidth, renderHeight, threadCount, drawCallCount, sceneTriangleCount,
		visTriangleCount, textureCount, textureByteCount, totalLightCount, totalDepthTests, totalColorWrites);
}

void SoftwareRenderer::submitFrame(const RenderCamera &camera, BufferView<const RenderDrawCall> drawCalls,
	const RenderFrameSettings &settings, uint32_t *outputBuffer)
{
	const int frameBufferWidth = this->paletteIndexBuffer.getWidth();
	const int frameBufferHeight = this->paletteIndexBuffer.getHeight();

	if (this->ditheringMode != settings.ditheringMode)
	{
		this->ditheringMode = settings.ditheringMode;
		swRender::CreateDitherBuffer(this->ditherBuffer, frameBufferWidth, frameBufferHeight, settings.ditheringMode);
	}

	BufferView2D<uint8_t> paletteIndexBufferView(this->paletteIndexBuffer.begin(), frameBufferWidth, frameBufferHeight);
	BufferView2D<double> depthBufferView(this->depthBuffer.begin(), frameBufferWidth, frameBufferHeight);
	BufferView3D<const bool> ditherBufferView(this->ditherBuffer.begin(), frameBufferWidth, frameBufferHeight, this->ditherBuffer.getDepth());
	BufferView2D<uint32_t> colorBufferView(outputBuffer, frameBufferWidth, frameBufferHeight);

	// Palette for 8-bit -> 32-bit color conversion.
	const ObjectTexture &paletteTexture = this->objectTextures.get(settings.paletteTextureID);

	// Light table for shading/transparency look-ups.
	const ObjectTexture &lightTableTexture = this->objectTextures.get(settings.lightTableTextureID);

	// Sky texture for horizon reflection shader.
	const ObjectTexture &skyBgTexture = this->objectTextures.get(settings.skyBgTextureID);

	swRender::ClearFrameBuffers(paletteIndexBufferView, depthBufferView, colorBufferView);
	swGeometry::ClearProcessedTriangles();

	const int drawCallCount = drawCalls.getCount();
	swRender::g_totalDrawCallCount = drawCallCount;

	for (int i = 0; i < drawCallCount; i++)
	{
		const RenderDrawCall &drawCall = drawCalls.get(i);
		const UniformBuffer &transformBuffer = this->uniformBuffers.get(drawCall.transformBufferID);
		const RenderTransform &transform = transformBuffer.get<RenderTransform>(drawCall.transformIndex);
		const Matrix4d &viewMatrix = camera.viewMatrix;
		const Matrix4d &projectionMatrix = camera.projectionMatrix;
		
		Double3 preScaleTranslation = Double3::Zero;
		if (drawCall.preScaleTranslationBufferID >= 0)
		{
			const UniformBuffer &preScaleTranslationBuffer = this->uniformBuffers.get(drawCall.preScaleTranslationBufferID);
			preScaleTranslation = preScaleTranslationBuffer.get<Double3>(0);
		}
		
		const VertexBuffer &vertexBuffer = this->vertexBuffers.get(drawCall.vertexBufferID);
		const AttributeBuffer &texCoordBuffer = this->attributeBuffers.get(drawCall.texCoordBufferID);
		const IndexBuffer &indexBuffer = this->indexBuffers.get(drawCall.indexBufferID);
		const ObjectTextureID *varyingTexture0 = drawCall.varyingTextures[0];
		const ObjectTextureID *varyingTexture1 = drawCall.varyingTextures[1];
		const ObjectTextureID textureID0 = (varyingTexture0 != nullptr) ? *varyingTexture0 : drawCall.textureIDs[0];
		const ObjectTextureID textureID1 = (varyingTexture1 != nullptr) ? *varyingTexture1 : drawCall.textureIDs[1];
		const VertexShaderType vertexShaderType = drawCall.vertexShaderType;
		const swGeometry::TriangleDrawListIndices drawListIndices = swGeometry::ProcessMeshForRasterization(
			preScaleTranslation, transform.translation, transform.rotation, transform.scale, viewMatrix,
			projectionMatrix, vertexBuffer, texCoordBuffer, indexBuffer, textureID0, textureID1, vertexShaderType);

		const TextureSamplingType textureSamplingType0 = drawCall.textureSamplingTypes[0];
		const TextureSamplingType textureSamplingType1 = drawCall.textureSamplingTypes[1];

		const RenderLightingType lightingType = drawCall.lightingType;
		double meshLightPercent = 0.0;
		const Light *lightPtrs[RenderLightIdList::MAX_LIGHTS];
		BufferView<const Light*> lightsView;
		if (lightingType == RenderLightingType::PerMesh)
		{
			meshLightPercent = drawCall.lightPercent;
		}
		else if (lightingType == RenderLightingType::PerPixel)
		{
			for (int lightIndex = 0; lightIndex < drawCall.lightIdCount; lightIndex++)
			{
				DebugAssertIndex(drawCall.lightIDs, lightIndex);
				const RenderLightID lightID = drawCall.lightIDs[lightIndex];
				lightPtrs[lightIndex] = &this->lights.get(lightID);
			}

			lightsView.init(lightPtrs, drawCall.lightIdCount);
		}

		const double ambientPercent = settings.ambientPercent;
		const PixelShaderType pixelShaderType = drawCall.pixelShaderType;
		const double pixelShaderParam0 = drawCall.pixelShaderParam0;
		const bool enableDepthRead = drawCall.enableDepthRead;
		const bool enableDepthWrite = drawCall.enableDepthWrite;
		const int ditheringMode = settings.ditheringMode;
		swRender::RasterizeTriangles(drawListIndices, textureSamplingType0, textureSamplingType1, lightingType, meshLightPercent,
			ambientPercent, lightsView, pixelShaderType, pixelShaderParam0, enableDepthRead, enableDepthWrite, this->objectTextures,
			paletteTexture, lightTableTexture, skyBgTexture, ditheringMode, camera, paletteIndexBufferView, depthBufferView,
			ditherBufferView, colorBufferView);
	}
}

void SoftwareRenderer::present()
{
	// Do nothing for now, might change later.
}
