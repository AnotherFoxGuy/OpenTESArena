#include "CommonUiView.h"
#include "LoadSavePanel.h"
#include "LoadSaveUiController.h"
#include "LoadSaveUiModel.h"
#include "LoadSaveUiView.h"
#include "../Game/Game.h"
#include "../Input/InputActionName.h"
#include "../UI/CursorData.h"
#include "../UI/FontLibrary.h"

LoadSavePanel::LoadSavePanel(Game &game)
	: Panel(game) { }

bool LoadSavePanel::init(LoadSavePanel::Type type)
{
	auto &game = this->getGame();
	auto &renderer = game.renderer;
	const auto &fontLibrary = FontLibrary::getInstance();

	// Populate save slots.
	const std::vector<LoadSaveUiModel::Entry> entries = LoadSaveUiModel::getSaveEntries(game);
	for (int i = 0; i < static_cast<int>(entries.size()); i++)
	{
		const LoadSaveUiModel::Entry &entry = entries[i];
		const std::string &text = entry.displayText;
		const TextBox::InitInfo textBoxInitInfo = TextBox::InitInfo::makeWithCenter(
			text,
			LoadSaveUiView::getEntryCenterPoint(i),
			LoadSaveUiView::EntryFontName,
			LoadSaveUiView::getEntryTextColor(),
			LoadSaveUiView::EntryTextAlignment,
			fontLibrary);

		TextBox textBox;
		if (!textBox.init(textBoxInitInfo, text, renderer))
		{
			DebugLogError("Couldn't init load/save text box " + std::to_string(i) + ".");
			continue;
		}

		this->saveTextBoxes.emplace_back(std::move(textBox));
	}

	// Each save slot is a button proxy with its own callback based on its index.
	for (int i = 0; i < LoadSaveUiModel::SlotCount; i++)
	{
		const Rect slotRect = LoadSaveUiModel::getSlotRect(i);
		this->addButtonProxy(MouseButtonType::Left, slotRect,
			[this, &game, i]() { LoadSaveUiController::onEntryButtonSelected(game, i); });
	}

	this->addInputActionListener(InputActionName::Back, LoadSaveUiController::onBackInputAction);

	auto &textureManager = game.textureManager;
	UiTextureID backgroundTextureID = LoadSaveUiView::allocBackgroundTexture(textureManager, renderer);
	this->backgroundTextureRef.init(backgroundTextureID, renderer);
	this->addDrawCall(
		this->backgroundTextureRef.get(),
		Int2::Zero,
		Int2(this->backgroundTextureRef.getWidth(), this->backgroundTextureRef.getHeight()),
		PivotType::TopLeft);

	for (int i = 0; i < static_cast<int>(this->saveTextBoxes.size()); i++)
	{
		UiDrawCall::TextureFunc textBoxTextureFunc = [this, i]()
		{
			DebugAssertIndex(this->saveTextBoxes, i);
			TextBox &textBox = this->saveTextBoxes[i];
			return textBox.getTextureID();
		};

		const TextBox &textBox = this->saveTextBoxes[i];
		const Rect &textBoxRect = textBox.getRect();
		this->addDrawCall(
			textBoxTextureFunc,
			textBoxRect.getCenter(),
			Int2(textBoxRect.getWidth(), textBoxRect.getHeight()),
			PivotType::Middle);
	}

	const UiTextureID cursorTextureID = CommonUiView::allocDefaultCursorTexture(textureManager, renderer);
	this->cursorTextureRef.init(cursorTextureID, renderer);
	this->addCursorDrawCall(this->cursorTextureRef.get(), PivotType::TopLeft);

	this->type = type;
	return true;
}
