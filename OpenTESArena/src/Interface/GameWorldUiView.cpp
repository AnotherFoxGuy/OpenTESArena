#include <algorithm>

#include "GameWorldPanel.h"
#include "GameWorldUiModel.h"
#include "GameWorldUiView.h"
#include "../Assets/ArenaPaletteName.h"
#include "../Assets/ArenaPortraitUtils.h"
#include "../Assets/ArenaTextureName.h"
#include "../Collision/RayCastTypes.h"
#include "../Entities/EntityDefinitionLibrary.h"
#include "../Game/Game.h"
#include "../Math/Constants.h"
#include "../Stats/CharacterClassLibrary.h"
#include "../Stats/CharacterRaceLibrary.h"
#include "../UI/ArenaFontName.h"
#include "../UI/FontLibrary.h"
#include "../UI/GuiUtils.h"
#include "../UI/Surface.h"

#include "components/utilities/String.h"

Rect GameWorldUiView::scaleClassicCursorRectToNative(int rectIndex, double xScale, double yScale)
{
	DebugAssertIndex(GameWorldUiView::CursorRegions, rectIndex);
	const Rect &classicRect = GameWorldUiView::CursorRegions[rectIndex];
	return Rect(
		static_cast<int>(std::ceil(static_cast<double>(classicRect.getLeft()) * xScale)),
		static_cast<int>(std::ceil(static_cast<double>(classicRect.getTop()) * yScale)),
		static_cast<int>(std::ceil(static_cast<double>(classicRect.getWidth()) * xScale)),
		static_cast<int>(std::ceil(static_cast<double>(classicRect.getHeight()) * yScale)));
}

TextBox::InitInfo GameWorldUiView::getPlayerNameTextBoxInitInfo(const std::string_view text,
	const FontLibrary &fontLibrary)
{
	return TextBox::InitInfo::makeWithXY(
		text,
		GameWorldUiView::PlayerNameTextBoxX,
		GameWorldUiView::PlayerNameTextBoxY,
		GameWorldUiView::PlayerNameFontName,
		GameWorldUiView::PlayerNameTextColor,
		GameWorldUiView::PlayerNameTextAlignment,
		fontLibrary);
}

Rect GameWorldUiView::getCharacterSheetButtonRect()
{
	return Rect(14, 166, 40, 29);
}

Rect GameWorldUiView::getPlayerPortraitRect()
{
	return GameWorldUiView::getCharacterSheetButtonRect();
}

Rect GameWorldUiView::getWeaponSheathButtonRect()
{
	return Rect(88, 151, 29, 22);
}

Rect GameWorldUiView::getStealButtonRect()
{
	return Rect(147, 151, 29, 22);
}

Rect GameWorldUiView::getStatusButtonRect()
{
	return Rect(177, 151, 29, 22);
}

Rect GameWorldUiView::getMagicButtonRect()
{
	return Rect(88, 175, 29, 22);
}

Rect GameWorldUiView::getLogbookButtonRect()
{
	return Rect(118, 175, 29, 22);
}

Rect GameWorldUiView::getUseItemButtonRect()
{
	return Rect(147, 175, 29, 22);
}

Rect GameWorldUiView::getCampButtonRect()
{
	return Rect(177, 175, 29, 22);
}

Rect GameWorldUiView::getScrollUpButtonRect()
{
	return Rect(208, (ArenaRenderUtils::SCREEN_HEIGHT - 53) + 3, 9, 9);
}

Rect GameWorldUiView::getScrollDownButtonRect()
{
	return Rect(208, (ArenaRenderUtils::SCREEN_HEIGHT - 53) + 44, 9, 9);
}

Rect GameWorldUiView::getMapButtonRect()
{
	return Rect(118, 151, 29, 22);
}

Rect GameWorldUiView::getButtonRect(GameWorldUiModel::ButtonType buttonType)
{
	switch (buttonType)
	{
	case GameWorldUiModel::ButtonType::CharacterSheet:
		return GameWorldUiView::getCharacterSheetButtonRect();
	case GameWorldUiModel::ButtonType::ToggleWeapon:
		return GameWorldUiView::getWeaponSheathButtonRect();
	case GameWorldUiModel::ButtonType::Map:
		return GameWorldUiView::getMapButtonRect();
	case GameWorldUiModel::ButtonType::Steal:
		return GameWorldUiView::getStealButtonRect();
	case GameWorldUiModel::ButtonType::Status:
		return GameWorldUiView::getStatusButtonRect();
	case GameWorldUiModel::ButtonType::Magic:
		return GameWorldUiView::getMagicButtonRect();
	case GameWorldUiModel::ButtonType::Logbook:
		return GameWorldUiView::getLogbookButtonRect();
	case GameWorldUiModel::ButtonType::UseItem:
		return GameWorldUiView::getUseItemButtonRect();
	case GameWorldUiModel::ButtonType::Camp:
		return GameWorldUiView::getCampButtonRect();
	default:
		DebugUnhandledReturnMsg(Rect, std::to_string(static_cast<int>(buttonType)));
	}
}

Int2 GameWorldUiView::getStatusPopUpTextCenterPoint(Game &game)
{
	return GameWorldUiView::getInterfaceCenter(game);
}

int GameWorldUiView::getStatusPopUpTextureWidth(int textWidth)
{
	return textWidth + 12;
}

int GameWorldUiView::getStatusPopUpTextureHeight(int textHeight)
{
	return textHeight + 12;
}

Int2 GameWorldUiView::getGameWorldInterfacePosition()
{
	return Int2(ArenaRenderUtils::SCREEN_WIDTH / 2, ArenaRenderUtils::SCREEN_HEIGHT);
}

Int2 GameWorldUiView::getNoMagicTexturePosition()
{
	return Int2(91, 177);
}

Int2 GameWorldUiView::getTriggerTextPosition(Game &game, int gameWorldInterfaceTextureHeight)
{
	const auto &options = game.options;
	const bool modernInterface = options.getGraphics_ModernInterface();

	const int textX = ArenaRenderUtils::SCREEN_WIDTH / 2;

	const int interfaceOffsetY = modernInterface ? (gameWorldInterfaceTextureHeight / 2) : gameWorldInterfaceTextureHeight;
	const int textY = ArenaRenderUtils::SCREEN_HEIGHT - interfaceOffsetY - 3;

	return Int2(textX, textY);
}

Int2 GameWorldUiView::getActionTextPosition()
{
	const int textX = ArenaRenderUtils::SCREEN_WIDTH / 2;
	const int textY = 20;
	return Int2(textX, textY);
}

Int2 GameWorldUiView::getEffectTextPosition()
{
	// @todo
	return Int2::Zero;
}

double GameWorldUiView::getTriggerTextSeconds(const std::string_view text)
{
	return std::max(2.50, static_cast<double>(text.size()) * 0.050);
}

double GameWorldUiView::getActionTextSeconds(const std::string_view text)
{
	return std::max(2.25, static_cast<double>(text.size()) * 0.050);
}

double GameWorldUiView::getEffectTextSeconds(const std::string_view text)
{
	return std::max(2.50, static_cast<double>(text.size()) * 0.050);
}

TextBox::InitInfo GameWorldUiView::getTriggerTextBoxInitInfo(const FontLibrary &fontLibrary)
{
	std::string dummyText;
	for (int i = 0; i < 4; i++)
	{
		std::string dummyLine(40, TextRenderUtils::LARGEST_CHAR); // Arbitrary worst-case line size.
		dummyText += dummyLine + '\n';
	}

	const TextRenderUtils::TextShadowInfo shadow(
		GameWorldUiView::TriggerTextShadowOffsetX,
		GameWorldUiView::TriggerTextShadowOffsetY,
		GameWorldUiView::TriggerTextShadowColor);

	return TextBox::InitInfo::makeWithCenter(
		dummyText,
		Int2::Zero, // @todo: needs to be a variable due to classic/modern mode. Maybe make two text boxes?
		GameWorldUiView::TriggerTextFontName,
		GameWorldUiView::TriggerTextColor,
		GameWorldUiView::TriggerTextAlignment,
		shadow,
		GameWorldUiView::TriggerTextLineSpacing,
		fontLibrary);
}

TextBox::InitInfo GameWorldUiView::getActionTextBoxInitInfo(const FontLibrary &fontLibrary)
{
	std::string dummyText;
	for (int i = 0; i < 2; i++)
	{
		std::string dummyLine(35, TextRenderUtils::LARGEST_CHAR); // Arbitrary worst-case line size.
		dummyText += dummyLine + '\n';
	}

	const TextRenderUtils::TextShadowInfo shadow(
		GameWorldUiView::ActionTextShadowOffsetX,
		GameWorldUiView::ActionTextShadowOffsetY,
		GameWorldUiView::ActionTextShadowColor);

	return TextBox::InitInfo::makeWithCenter(
		dummyText,
		Int2::Zero, // @todo: needs to be a variable due to classic/modern mode. Maybe make two text boxes?
		GameWorldUiView::ActionTextFontName,
		GameWorldUiView::ActionTextColor,
		GameWorldUiView::ActionTextAlignment,
		shadow,
		0,
		fontLibrary);
}

TextBox::InitInfo GameWorldUiView::getEffectTextBoxInitInfo(const FontLibrary &fontLibrary)
{
	DebugNotImplemented();
	return TextBox::InitInfo();
}

Int2 GameWorldUiView::getTooltipPosition(Game &game)
{
	DebugAssert(!game.options.getGraphics_ModernInterface());

	const int x = 0;
	const int y = ArenaRenderUtils::SCREEN_HEIGHT - GameWorldUiView::UiBottomRegion.getHeight();
	return Int2(x, y);
}

Rect GameWorldUiView::getCompassClipRect()
{
	constexpr int width = 32;
	constexpr int height = 7;
	return Rect(
		(ArenaRenderUtils::SCREEN_WIDTH / 2) - (width / 2),
		height,
		width,
		height);
}

Int2 GameWorldUiView::getCompassSliderPosition(Game &game, const VoxelDouble2 &playerDirection)
{
	const double angle = GameWorldUiModel::getCompassAngle(playerDirection);

	// Offset in the "slider" texture. Due to how SLIDER.IMG is drawn, there's a small "pop-in" when turning from
	// N to NE, because N is drawn in two places, but the second place (offset == 256) has tick marks where "NE"
	// should be.
	const int xOffset = static_cast<int>(240.0 + std::round(256.0 * (angle / (2.0 * Constants::Pi)))) % 256;
	const Rect clipRect = GameWorldUiView::getCompassClipRect();
	return clipRect.getTopLeft() - Int2(xOffset, 0);
}

Int2 GameWorldUiView::getCompassFramePosition()
{
	return Int2(ArenaRenderUtils::SCREEN_WIDTH / 2, 0);
}

Int2 GameWorldUiView::getWeaponAnimationOffset(const std::string &weaponFilename, int frameIndex,
	TextureManager &textureManager)
{
	// @todo: this is obsoleted by WeaponAnimationDefinition

	const std::optional<TextureFileMetadataID> metadataID = textureManager.tryGetMetadataID(weaponFilename.c_str());
	if (!metadataID.has_value())
	{
		DebugCrash("Couldn't get weapon animation metadata from \"" + weaponFilename + "\".");
	}

	const TextureFileMetadata &textureFileMetadata = textureManager.getMetadataHandle(*metadataID);
	return textureFileMetadata.getOffset(frameIndex);
}

Int2 GameWorldUiView::getInterfaceCenter(Game &game)
{
	const bool modernInterface = game.options.getGraphics_ModernInterface();
	if (modernInterface)
	{
		return Int2(ArenaRenderUtils::SCREEN_WIDTH / 2, ArenaRenderUtils::SCREEN_HEIGHT / 2);
	}
	else
	{
		return Int2(
			ArenaRenderUtils::SCREEN_WIDTH / 2,
			(ArenaRenderUtils::SCREEN_HEIGHT - GameWorldUiView::UiBottomRegion.getHeight()) / 2);
	}
}

Int2 GameWorldUiView::getNativeWindowCenter(const Renderer &renderer)
{
	const Int2 windowDims = renderer.getWindowDimensions();
	const Int2 nativeCenter = windowDims / 2;
	return nativeCenter;
}

TextureAsset GameWorldUiView::getPaletteTextureAsset()
{
	return TextureAsset(std::string(ArenaPaletteName::Default));
}

TextureAsset GameWorldUiView::getGameWorldInterfaceTextureAsset()
{
	return TextureAsset(std::string(ArenaTextureName::GameWorldInterface));
}

TextureAsset GameWorldUiView::getStatusGradientTextureAsset(StatusGradientType gradientType)
{
	const int gradientID = static_cast<int>(gradientType);
	return TextureAsset(std::string(ArenaTextureName::StatusGradients), gradientID);
}

TextureAsset GameWorldUiView::getPlayerPortraitTextureAsset(bool isMale, int raceID, int portraitID)
{
	const CharacterRaceLibrary &characterRaceLibrary = CharacterRaceLibrary::getInstance();
	const CharacterRaceDefinition &characterRaceDefinition = characterRaceLibrary.getDefinition(raceID);

	if (isMale)
	{
		return TextureAsset(std::string(characterRaceDefinition.maleGameUiHeadsFilename), portraitID);
	}
	else
	{
		return TextureAsset(std::string(characterRaceDefinition.femaleGameUiHeadsFilename), portraitID);
	}
}

TextureAsset GameWorldUiView::getNoMagicTextureAsset()
{
	return TextureAsset(std::string(ArenaTextureName::NoSpell));
}

TextureAsset GameWorldUiView::getWeaponAnimTextureAsset(const std::string &weaponFilename, int index)
{
	// @todo: this is obsoleted by WeaponAnimationDefinition
	return TextureAsset(std::string(weaponFilename), index);
}

TextureAsset GameWorldUiView::getCompassFrameTextureAsset()
{
	return TextureAsset(std::string(ArenaTextureName::CompassFrame));
}

TextureAsset GameWorldUiView::getCompassSliderTextureAsset()
{
	return TextureAsset(std::string(ArenaTextureName::CompassSlider));
}

TextureAsset GameWorldUiView::getArrowCursorTextureAsset(int cursorIndex)
{
	return TextureAsset(std::string(ArenaTextureName::ArrowCursors), cursorIndex);
}

UiTextureID GameWorldUiView::allocGameWorldInterfaceTexture(TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getGameWorldInterfaceTextureAsset();
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for game world interface.");
	}

	return textureID;
}

UiTextureID GameWorldUiView::allocStatusGradientTexture(StatusGradientType gradientType,
	TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getStatusGradientTextureAsset(gradientType);
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for status gradient " + std::to_string(static_cast<int>(gradientType)) + ".");
	}

	return textureID;
}

UiTextureID GameWorldUiView::allocPlayerPortraitTexture(bool isMale, int raceID, int portraitID,
	TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getPlayerPortraitTextureAsset(isMale, raceID, portraitID);
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for player portrait (male: " + std::to_string(static_cast<int>(isMale)) +
			", race: " + std::to_string(raceID) + ", portrait: " + std::to_string(portraitID) + ").");
	}

	return textureID;
}

UiTextureID GameWorldUiView::allocNoMagicTexture(TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getNoMagicTextureAsset();
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for no magic icon.");
	}

	return textureID;
}

UiTextureID GameWorldUiView::allocWeaponAnimTexture(const std::string &weaponFilename, int index,
	TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getWeaponAnimTextureAsset(weaponFilename, index);
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for weapon animation \"" + weaponFilename +
			"\" index " + std::to_string(index) + ".");
	}

	return textureID;
}

UiTextureID GameWorldUiView::allocCompassFrameTexture(TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getCompassFrameTextureAsset();
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for compass frame.");
	}

	return textureID;
}

UiTextureID GameWorldUiView::allocCompassSliderTexture(TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getCompassSliderTextureAsset();
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for compass frame.");
	}

	return textureID;
}

UiTextureID GameWorldUiView::allocTooltipTexture(GameWorldUiModel::ButtonType buttonType,
	const FontLibrary &fontLibrary, Renderer &renderer)
{
	const std::string text = GameWorldUiModel::getButtonTooltip(buttonType);
	const Surface surface = TextureUtils::createTooltip(text, fontLibrary);
	const BufferView2D<const uint32_t> pixelsView(static_cast<const uint32_t*>(surface.getPixels()),
		surface.getWidth(), surface.getHeight());

	UiTextureID id;
	if (!renderer.tryCreateUiTexture(pixelsView, &id))
	{
		DebugCrash("Couldn't create tooltip texture for \"" + text + "\".");
	}

	return id;
}

UiTextureID GameWorldUiView::allocArrowCursorTexture(int cursorIndex, TextureManager &textureManager, Renderer &renderer)
{
	const TextureAsset textureAsset = GameWorldUiView::getArrowCursorTextureAsset(cursorIndex);
	const TextureAsset paletteTextureAsset = GameWorldUiView::getPaletteTextureAsset();

	UiTextureID textureID;
	if (!TextureUtils::tryAllocUiTexture(textureAsset, paletteTextureAsset, textureManager, renderer, &textureID))
	{
		DebugCrash("Couldn't create UI texture for arrow cursor " + std::to_string(cursorIndex) + ".");
	}

	return textureID;
}

// @temp: keep until 3D-DDA ray casting is fully correct (i.e. entire ground is red dots for
// levels where ceilingScale < 1.0, and same with ceiling blue dots).
// @todo: As of SDL 2.0.10 which introduced batching, this now behaves like the color is per frame, not per call, which isn't correct, and flushing doesn't help.
void GameWorldUiView::DEBUG_ColorRaycastPixel(Game &game)
{
	auto &renderer = game.renderer;
	const int selectionDim = 3;
	const Int2 windowDims = renderer.getWindowDimensions();

	constexpr int xOffset = 16;
	constexpr int yOffset = 16;

	const auto &gameState = game.gameState;
	if (!gameState.isActiveMapValid())
	{
		return;
	}

	const auto &player = game.player;
	const CoordDouble3 rayStart = player.getEyeCoord();
	const Double3 &cameraDirection = player.forward;
	const double viewAspectRatio = renderer.getViewAspect();

	const double ceilingScale = gameState.getActiveCeilingScale();
	const SceneManager &sceneManager = game.sceneManager;
	const VoxelChunkManager &voxelChunkManager = sceneManager.voxelChunkManager;
	const EntityChunkManager &entityChunkManager = sceneManager.entityChunkManager;
	const CollisionChunkManager &collisionChunkManager = sceneManager.collisionChunkManager;

	for (int y = 0; y < windowDims.y; y += yOffset)
	{
		for (int x = 0; x < windowDims.x; x += xOffset)
		{
			const Double3 rayDirection = GameWorldUiModel::screenToWorldRayDirection(game, Int2(x, y));

			// Not registering entities with ray cast hits for efficiency since this debug visualization is for voxels.
			constexpr bool includeEntities = false;
			RayCastHit hit;
			const bool success = Physics::rayCast(rayStart, rayDirection, ceilingScale, cameraDirection,
				includeEntities, voxelChunkManager, entityChunkManager, collisionChunkManager,
				EntityDefinitionLibrary::getInstance(), hit);

			if (success)
			{
				Color color;
				switch (hit.type)
				{
				case RayCastHitType::Voxel:
				{
					const Color colors[] = { Color::Red, Color::Green, Color::Blue, Color::Cyan, Color::Yellow };
					const VoxelInt3 &voxel = hit.voxelHit.voxel;
					const int colorsIndex = std::clamp<int>(voxel.y, 0, std::size(colors) - 1);
					DebugAssertIndex(colors, colorsIndex);
					color = colors[colorsIndex];
					break;
				}
				case RayCastHitType::Entity:
				{
					color = Color::Yellow;
					break;
				}
				}

				renderer.drawRect(color, x, y, selectionDim, selectionDim);
			}
		}
	}
}

// @temp: keep until 3D-DDA ray casting is fully correct (i.e. entire ground is red dots for
// levels where ceilingScale < 1.0, and same with ceiling blue dots).
void GameWorldUiView::DEBUG_PhysicsRaycast(Game &game)
{
	// ray cast out from center and display hit info (faster/better than console logging).
	GameWorldUiView::DEBUG_ColorRaycastPixel(game);

	const auto &options = game.options;
	const auto &player = game.player;
	const Double3 &cameraDirection = player.forward;

	auto &renderer = game.renderer;
	const Int2 viewDims = renderer.getViewDimensions();
	const Int2 viewCenterPoint(viewDims.x / 2, viewDims.y / 2);

	const CoordDouble3 rayStart = player.getEyeCoord();
	const VoxelDouble3 rayDirection = GameWorldUiModel::screenToWorldRayDirection(game, viewCenterPoint);

	const SceneManager &sceneManager = game.sceneManager;
	const VoxelChunkManager &voxelChunkManager = sceneManager.voxelChunkManager;
	const EntityChunkManager &entityChunkManager = sceneManager.entityChunkManager;
	const CollisionChunkManager &collisionChunkManager = sceneManager.collisionChunkManager;

	const auto &gameState = game.gameState;
	const double ceilingScale = gameState.getActiveCeilingScale();

	EntityDefinitionLibrary &entityDefLibrary = EntityDefinitionLibrary::getInstance();

	constexpr bool includeEntities = true;
	RayCastHit hit;
	const bool success = Physics::rayCast(rayStart, rayDirection, ceilingScale, cameraDirection, includeEntities,
		voxelChunkManager, entityChunkManager, collisionChunkManager, entityDefLibrary, hit);

	std::string text;
	if (success)
	{
		switch (hit.type)
		{
		case RayCastHitType::Voxel:
		{
			const ChunkInt2 chunkPos = hit.coord.chunk;
			const VoxelChunk &chunk = voxelChunkManager.getChunkAtPosition(chunkPos);

			const RayCastVoxelHit &voxelHit = hit.voxelHit;
			const VoxelInt3 &voxel = voxelHit.voxel;
			const VoxelTraitsDefID voxelTraitsDefID = chunk.getTraitsDefID(voxel.x, voxel.y, voxel.z);
			const VoxelTraitsDefinition &voxelTraitsDef = chunk.getTraitsDef(voxelTraitsDefID);

			text = "Voxel: (" + voxel.toString() + "), " + std::to_string(static_cast<int>(voxelTraitsDef.type)) + ' ' + std::to_string(hit.t);
			break;
		}
		case RayCastHitType::Entity:
		{
			const RayCastEntityHit &entityHit = hit.entityHit;
			const auto &exeData = BinaryAssetLibrary::getInstance().getExeData();

			// Try inspecting the entity (can be from any distance). If they have a display name, then show it.
			const EntityInstance &entityInst = entityChunkManager.getEntity(entityHit.id);
			const EntityDefinition &entityDef = entityChunkManager.getEntityDef(entityInst.defID);
			const auto &charClassLibrary = CharacterClassLibrary::getInstance();

			std::string entityName;
			if (EntityUtils::tryGetDisplayName(entityDef, charClassLibrary, &entityName))
			{
				text = std::move(entityName);
			}
			else
			{
				// Placeholder text for testing.
				text = "Entity " + std::to_string(entityHit.id);
			}

			text.append(' ' + std::to_string(hit.t));
			break;
		}
		default:
			text.append("Unknown hit type");
			break;
		}
	}
	else
	{
		text = "No hit";
	}

	const TextBox::InitInfo textBoxInitInfo = TextBox::InitInfo::makeWithXY(
		text,
		0,
		0,
		ArenaFontName::Arena,
		Color::White,
		TextAlignment::TopLeft,
		FontLibrary::getInstance());

	TextBox textBox;
	if (!textBox.init(textBoxInitInfo, text, renderer))
	{
		DebugCrash("Couldn't init physics ray cast text box.");
	}

	const int originalX = ArenaRenderUtils::SCREEN_WIDTH / 2;
	const int originalY = (ArenaRenderUtils::SCREEN_HEIGHT / 2) + 10;
	DebugNotImplemented(); // Disabled for now until I need it again
	//renderer.drawOriginal(textBox.getTextureID(), originalX, originalY);
}

void GameWorldUiView::DEBUG_DrawVoxelVisibilityQuadtree(Game &game)
{
	Renderer &renderer = game.renderer;
	constexpr int quadtreeTextureDim = ChunkUtils::CHUNK_DIM;
	UiTextureID quadtreeTextureID;
	if (!renderer.tryCreateUiTexture(quadtreeTextureDim, quadtreeTextureDim, &quadtreeTextureID))
	{
		DebugLogError("Couldn't allocate voxel visibility quadtree debug texture.");
		return;
	}

	ScopedUiTextureRef quadtreeTextureRef(quadtreeTextureID, renderer);
	const Int2 quadtreeTextureDims(quadtreeTextureRef.getWidth(), quadtreeTextureRef.getHeight());

	const SceneManager &sceneManager = game.sceneManager;
	const Player &player = game.player;
	const CoordDouble3 playerCoord = player.getEyeCoord();
	const CoordInt3 playerVoxelCoord(playerCoord.chunk, VoxelUtils::pointToVoxel(playerCoord.point));
	const VoxelVisibilityChunkManager &voxelVisChunkManager = sceneManager.voxelVisChunkManager;
	const VoxelVisibilityChunk *playerVoxelVisChunk = voxelVisChunkManager.tryGetChunkAtPosition(playerCoord.chunk);
	if (playerVoxelVisChunk != nullptr)
	{
		const uint32_t visibleColor = Color::Green.toARGB();
		const uint32_t partialColor = Color::Yellow.toARGB();
		const uint32_t invisibleColor = Color::Red.toARGB();
		const uint32_t playerColor = Color::Magenta.toARGB();

		uint32_t *quadtreeTexels = quadtreeTextureRef.lockTexels();

		for (int y = 0; y < quadtreeTextureDims.y; y++)
		{
			for (int x = 0; x < quadtreeTextureDims.x; x++)
			{
				const int treeLevelSideLength = VoxelVisibilityChunk::NODES_PER_SIDE[VoxelVisibilityChunk::TREE_LEVEL_INDEX_LEAF];
				const SNInt internalX = (y * treeLevelSideLength) / ChunkUtils::CHUNK_DIM;
				const WEInt internalZ = (x * treeLevelSideLength) / ChunkUtils::CHUNK_DIM;
				const int internalIndex = internalX + (internalZ * treeLevelSideLength);
				DebugAssertIndex(playerVoxelVisChunk->leafNodeFrustumTests, internalIndex);
				VisibilityType visibilityType = playerVoxelVisChunk->leafNodeFrustumTests[internalIndex] ? VisibilityType::Inside : VisibilityType::Outside;

				const int dstIndex = ((quadtreeTextureDims.x - 1) - x) + (y * quadtreeTextureDims.x);
				uint32_t color = 0;

				const bool inPlayerVoxel = (y == playerVoxelCoord.voxel.x) && (x == playerVoxelCoord.voxel.z);
				if (inPlayerVoxel)
				{
					color = playerColor;
				}
				else
				{
					switch (visibilityType)
					{
					case VisibilityType::Outside:
						color = invisibleColor;
						break;
					case VisibilityType::Partial:
						color = partialColor;
						break;
					case VisibilityType::Inside:
						color = visibleColor;
						break;
					}
				}

				quadtreeTexels[dstIndex] = color;
			}
		}

		quadtreeTextureRef.unlockTexels();
	}

	const Int2 position(ArenaRenderUtils::SCREEN_WIDTH, 0);
	const Int2 size = quadtreeTextureDims;
	const Int2 windowDims = renderer.getWindowDimensions();
	constexpr PivotType pivotType = PivotType::TopRight;
	constexpr RenderSpace renderSpace = RenderSpace::Classic;

	double xPercent, yPercent, wPercent, hPercent;
	GuiUtils::makeRenderElementPercents(position.x, position.y, size.x, size.y, windowDims.x, windowDims.y,
		renderSpace, pivotType, &xPercent, &yPercent, &wPercent, &hPercent);

	const RendererSystem2D::RenderElement renderElement(quadtreeTextureID, xPercent, yPercent, wPercent, hPercent);
	renderer.draw(&renderElement, 1, renderSpace);
}
