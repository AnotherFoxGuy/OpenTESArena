#include "MessageBoxSubPanel.h"
#include "../Assets/ArenaPaletteName.h"
#include "../Assets/ArenaTextureName.h"
#include "../Game/Game.h"
#include "../Math/Rect.h"
#include "../Media/TextureManager.h"
#include "../Rendering/Renderer.h"
#include "../UI/CursorAlignment.h"
#include "../UI/CursorData.h"

MessageBoxSubPanel::MessageBoxSubPanel(Game &game)
	: Panel(game) { }

bool MessageBoxSubPanel::init(MessageBoxSubPanel::Title &&title, std::vector<MessageBoxSubPanel::Element> &&elements,
	const std::function<void(Game&)> &cancelFunction)
{
	this->title = std::move(title);
	this->elements = std::move(elements);
	this->cancelFunction = cancelFunction;
	return true;
}

bool MessageBoxSubPanel::init(MessageBoxSubPanel::Title &&title, std::vector<MessageBoxSubPanel::Element> &&elements)
{
	return this->init(std::move(title), std::move(elements), [](Game&) { });
}

std::optional<CursorData> MessageBoxSubPanel::getCurrentCursor() const
{
	auto &game = this->getGame();
	auto &renderer = game.getRenderer();
	auto &textureManager = game.getTextureManager();

	const std::string &paletteFilename = ArenaPaletteName::Default;
	const std::optional<PaletteID> paletteID = textureManager.tryGetPaletteID(paletteFilename.c_str());
	if (!paletteID.has_value())
	{
		DebugLogWarning("Couldn't get palette ID for \"" + paletteFilename + "\".");
		return std::nullopt;
	}

	const std::string &textureFilename = ArenaTextureName::SwordCursor;
	const std::optional<TextureBuilderID> textureBuilderID =
		textureManager.tryGetTextureBuilderID(textureFilename.c_str());
	if (!textureBuilderID.has_value())
	{
		DebugLogWarning("Couldn't get texture builder ID for \"" + textureFilename + "\".");
		return std::nullopt;
	}

	return CursorData(*textureBuilderID, *paletteID, CursorAlignment::TopLeft);
}

void MessageBoxSubPanel::handleEvent(const SDL_Event &e)
{
	auto &game = this->getGame();
	const auto &inputManager = this->getGame().getInputManager();
	const bool escapePressed = inputManager.keyPressed(e, SDLK_ESCAPE);
	const bool leftClick = inputManager.mouseButtonPressed(e, SDL_BUTTON_LEFT);
	const bool rightClick = inputManager.mouseButtonPressed(e, SDL_BUTTON_RIGHT);

	if (escapePressed || rightClick)
	{
		this->cancelFunction(game);
	}
	else if (leftClick)
	{
		const Int2 mousePosition = inputManager.getMousePosition();
		const Int2 originalPoint = this->getGame().getRenderer()
			.nativeToOriginal(mousePosition);

		// See if any buttons were clicked.
		for (const auto &element : this->elements)
		{
			const Rect elementRect(
				element.textureX,
				element.textureY,
				element.texture.getWidth(),
				element.texture.getHeight());

			if (elementRect.contains(originalPoint))
			{
				element.function(game);
			}
		}
	}

	// @todo: custom hotkeys.
}

void MessageBoxSubPanel::render(Renderer &renderer)
{
	renderer.drawOriginal(this->title.texture, this->title.textureX, this->title.textureY);

	TextBox &titleTextBox = this->title.textBox;
	const Rect &titleTextBoxRect = titleTextBox.getRect();
	renderer.drawOriginal(titleTextBox.getTexture(), titleTextBoxRect.getLeft(), titleTextBoxRect.getTop());

	for (auto &element : this->elements)
	{
		renderer.drawOriginal(element.texture, element.textureX, element.textureY);

		TextBox &elementTextBox = element.textBox;
		const Rect &elementTextBoxRect = elementTextBox.getRect();
		renderer.drawOriginal(elementTextBox.getTexture(), elementTextBoxRect.getLeft(), elementTextBoxRect.getTop());
	}
}
