#include <algorithm>

#include "SDL.h"

#include "ListBox.h"
#include "RichTextString.h"
#include "TextAlignment.h"
#include "TextBox.h"
#include "../Math/Rect.h"
#include "../Math/Vector2.h"
#include "../Media/Color.h"
#include "../Media/Font.h"
#include "../Media/FontManager.h"
#include "../Rendering/Renderer.h"
#include "../Utilities/Debug.h"
#include "../Utilities/String.h"

ListBox::ListBox(int x, int y, const Color &textColor, const std::vector<std::string> &elements,
	FontName fontName, int maxDisplayed, FontManager &fontManager, Renderer &renderer)
	: textColor(textColor), point(x, y), fontName(fontName)
{
	DebugAssert(maxDisplayed > 0);

	this->scrollIndex = 0;

	// Get the font data associated with the font name.
	const Font &font = fontManager.getFont(fontName);

	this->characterHeight = font.getCharacterHeight();

	// Make text boxes for getting list box dimensions now and drawing later.
	// It's okay for there to be zero elements. Just be blank, then!
	for (const auto &element : elements)
	{
		// Remove any new lines.
		std::string trimmedElement = String::trimLines(element);

		const int textBoxX = 0;
		const int textBoxY = 0;

		const RichTextString richText(
			trimmedElement,
			font.getFontName(),
			textColor,
			TextAlignment::Left,
			fontManager);

		// Store the text box for later.
		auto textBox = std::make_unique<TextBox>(textBoxX, textBoxY, richText, renderer);
		this->textBoxes.push_back(std::move(textBox));
	}

	// Calculate the dimensions of the displayed list box area.
	const int width = [this]()
	{
		int maxWidth = 0;
		for (const auto &textBox : this->textBoxes)
		{
			const int textBoxWidth = textBox->getTexture().getWidth();

			if (textBoxWidth > maxWidth)
			{
				maxWidth = textBoxWidth;
			}
		}

		return maxWidth;
	}();

	const int height = font.getCharacterHeight() * maxDisplayed;

	// Create the clear surface. This exists because the text box surfaces can't
	// currently have an arbitrary size (otherwise they could extend to the end of 
	// each row), and because SDL_UpdateTexture requires pixels (so this avoids an 
	// allocation each time the update method is called).
	this->clearSurface = Surface::createWithFormat(width, height,
		Renderer::DEFAULT_BPP, Renderer::DEFAULT_PIXELFORMAT);
	this->clearSurface.fill(0, 0, 0, 0);

	// Create the visible texture. This will be updated when scrolling the list box.
	this->texture = renderer.createTexture(Renderer::DEFAULT_PIXELFORMAT,
		SDL_TEXTUREACCESS_STREAMING, width, height);
	SDL_SetTextureBlendMode(this->texture.get(), SDL_BLENDMODE_BLEND);

	// Draw the text boxes to the texture.
	this->updateDisplay();
}

int ListBox::getScrollIndex() const
{
	return this->scrollIndex;
}

int ListBox::getElementCount() const
{
	return static_cast<int>(this->textBoxes.size());
}

int ListBox::getMaxDisplayedCount() const
{
	return this->texture.getHeight() / this->characterHeight;
}

const Int2 &ListBox::getPoint() const
{
	return this->point;
}

const Texture &ListBox::getTexture() const
{
	return this->texture;
}

Int2 ListBox::getDimensions() const
{
	int w, h;
	SDL_QueryTexture(this->texture.get(), nullptr, nullptr, &w, &h);
	return Int2(w, h);
}

bool ListBox::contains(const Int2 &point)
{
	const Int2 dims = this->getDimensions();
	Rect rect(this->point.x, this->point.y, dims.x, dims.y);
	return rect.contains(point);
}

int ListBox::getClickedIndex(const Int2 &point) const
{
	// Only the Y component of the point really matters here.
	const int index = this->scrollIndex + 
		((point.y - this->point.y) / this->characterHeight);
	return index;
}

void ListBox::updateDisplay()
{
	// Clear the display texture. Otherwise, remnants of previous text might be left over.
	SDL_UpdateTexture(this->texture.get(), nullptr,
		this->clearSurface.get()->pixels, this->clearSurface.get()->pitch);

	// Prepare the range of text boxes that will be displayed.
	const int totalElements = static_cast<int>(this->textBoxes.size());
	const int maxDisplayed = this->getMaxDisplayedCount();
	const int indexEnd = std::min(this->scrollIndex + maxDisplayed, totalElements);

	// Draw the relevant text boxes according to scroll index.
	for (int i = this->scrollIndex; i < indexEnd; i++)
	{
		const Surface &surface = this->textBoxes.at(i)->getSurface();

		SDL_Rect rect;
		rect.x = 0;
		rect.y = (i - this->scrollIndex) * surface.getHeight();
		rect.w = surface.getWidth();
		rect.h = surface.getHeight();

		// Update the texture's pixels at the correct height offset.
		SDL_UpdateTexture(this->texture.get(), &rect, surface.get()->pixels, surface.get()->pitch);
	}
}

void ListBox::scrollUp()
{
	this->scrollIndex -= 1;
	this->updateDisplay();
}

void ListBox::scrollDown()
{
	this->scrollIndex += 1;
	this->updateDisplay();
}
