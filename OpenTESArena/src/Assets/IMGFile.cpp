#include <unordered_map>
#include <unordered_set>

#include "Compression.h"
#include "IMGFile.h"
#include "../Math/Vector2.h"
#include "../Media/Color.h"
#include "../Utilities/Bytes.h"
#include "../Utilities/Debug.h"

#include "components/vfs/manager.hpp"

namespace
{
	// These .IMG files are actually headerless/raw files with hardcoded dimensions.
	const std::unordered_map<std::string, Int2> RawImgOverride =
	{
		{ "ARENARW.IMG", { 16, 16 } },
		{ "CITY.IMG", { 16, 11 } },
		{ "DITHER.IMG", { 16, 50 } },
		{ "DITHER2.IMG", { 16, 50 } },
		{ "DUNGEON.IMG", { 14,  8 } },
		{ "DZTTAV.IMG", { 32, 34 } },
		{ "NOCAMP.IMG", { 25, 19 } },
		{ "NOSPELL.IMG", { 25, 19 } },
		{ "P1.IMG", { 320, 53 } },
		{ "POPTALK.IMG", { 320, 77 } },
		{ "S2.IMG", { 320, 36 } },
		{ "SLIDER.IMG", { 289,  7 } },
		{ "TOWN.IMG", { 9, 10 } },
		{ "UPDOWN.IMG", { 8, 16 } },
		{ "VILLAGE.IMG", { 8,  8 } }
	};

	// These .IMG filenames are misspelled, and Arena does not use them in-game.
	const std::unordered_set<std::string> MisspelledIMGs =
	{
		"SFOUNF1M.IMG",
		"SFOUNF1T.IMG"
	};
}

IMGFile::IMGFile(const std::string &filename)
{
	// There are a couple .INFs that reference misspelled .IMGs. Arena doesn't seem
	// to use them, so if they are requested here, just return a dummy image.
	if (MisspelledIMGs.find(filename) != MisspelledIMGs.end())
	{
		this->width = 1;
		this->height = 1;
		this->pixels = std::make_unique<uint8_t[]>(this->width * this->height);
		this->pixels[0] = 0;
		return;
	}

	VFS::IStreamPtr stream = VFS::Manager::get().open(filename);
	DebugAssert(stream != nullptr, "Could not open \"" + filename + "\".");

	stream->seekg(0, std::ios::end);
	std::vector<uint8_t> srcData(stream->tellg());
	stream->seekg(0, std::ios::beg);
	stream->read(reinterpret_cast<char*>(srcData.data()), srcData.size());

	uint16_t xoff, yoff, width, height, flags, len;

	// Read header data if not raw. Wall .IMGs have no header and are 4096 bytes.
	const auto rawOverride = RawImgOverride.find(filename);
	const bool isRaw = rawOverride != RawImgOverride.end();
	if (isRaw)
	{
		xoff = 0;
		yoff = 0;
		width = rawOverride->second.x;
		height = rawOverride->second.y;
		flags = 0;
		len = width * height;
	}
	else if (srcData.size() == 4096)
	{
		// Some wall .IMGs have rows of black (transparent) pixels near the 
		// beginning, so the header would just be zeroes. This is a guess to 
		// try and fix that issue as well as cover all other wall .IMGs.
		xoff = 0;
		yoff = 0;
		width = 64;
		height = 64;
		flags = 0;
		len = width * height;
	}
	else
	{
		// Read header data.
		xoff = Bytes::getLE16(srcData.data());
		yoff = Bytes::getLE16(srcData.data() + 2);
		width = Bytes::getLE16(srcData.data() + 4);
		height = Bytes::getLE16(srcData.data() + 6);
		flags = Bytes::getLE16(srcData.data() + 8);
		len = Bytes::getLE16(srcData.data() + 10);
	}

	const int headerSize = 12;

	// Read the .IMG's built-in palette if it has one.
	const bool hasBuiltInPalette = (flags & 0x0100) != 0;
	if (hasBuiltInPalette)
	{
		// Read the palette data.
		this->palette = std::make_unique<Palette>(
			IMGFile::readPalette(srcData.data() + headerSize + len));
	}

	// Lambda for setting members and constructing the final image.
	auto makeImage = [this](int width, int height, const uint8_t *data)
	{
		this->width = width;
		this->height = height;
		this->pixels = std::make_unique<uint8_t[]>(width * height);
		std::copy(data, data + (width * height), this->pixels.get());
	};

	// Decide how to use the pixel data.
	if (isRaw)
	{
		// Special case: DZTTAV.IMG is a raw image with hardcoded dimensions, but the game
		// expects it to be a 64x64 texture, so it needs its own case.
		if (filename == "DZTTAV.IMG")
		{
			this->width = 64;
			this->height = 64;
			this->pixels = std::make_unique<uint8_t[]>(this->width * this->height);
			std::fill(this->pixels.get(), this->pixels.get() + (this->width * this->height), 0);

			for (int y = 0; y < height; y++)
			{
				for (int x = 0; x < width; x++)
				{
					// Offset the destination X by 32 so it matches DZTTEP.IMG.
					const int srcIndex = x + (y * width);
					const int dstIndex = (x + 32) + (y * this->width);
					this->pixels[dstIndex] = *(srcData.data() + srcIndex);
				}
			}
		}
		else
		{
			// Uncompressed IMG with no header (excluding walls).
			makeImage(width, height, srcData.data());
		}
	}
	else if ((srcData.size() == 4096) && (width == 64) && (height == 64))
	{
		// Wall texture (the flags variable is garbage).
		makeImage(64, 64, srcData.data());
	}
	else
	{
		// Decode the pixel data according to the IMG flags.
		if ((flags & 0x00FF) == 0)
		{
			// Uncompressed IMG with header.
			makeImage(width, height, srcData.data() + headerSize);
		}
		else if ((flags & 0x00FF) == 0x0004)
		{
			// Type 4 compression.
			std::vector<uint8_t> decomp(width * height);
			Compression::decodeType04(srcData.begin() + headerSize,
				srcData.begin() + headerSize + len, decomp);

			// Create 32-bit image.
			makeImage(width, height, decomp.data());
		}
		else if ((flags & 0x00FF) == 0x0008)
		{
			// Type 8 compression. Contains a 2 byte decompressed length after
			// the header, so skip that (should be equivalent to width * height).
			std::vector<uint8_t> decomp(width * height);
			Compression::decodeType08(srcData.begin() + headerSize + 2,
				srcData.begin() + headerSize + len, decomp);

			// Create 32-bit image.
			makeImage(width, height, decomp.data());
		}
		else
		{
			DebugCrash("Unrecognized IMG \"" + filename + "\".");
		}
	}
}

Palette IMGFile::readPalette(const uint8_t *paletteData)
{
	// The palette data is 768 bytes, starting after the pixel data ends.
	// Unlike COL files, embedded palettes are stored with components in
	// the range of 0...63 rather than 0...255 (this was because old VGA
	// hardware only had 6-bit DACs, giving a maximum intensity value of
	// 63, while newer hardware had 8-bit DACs for up to 255.
	Palette palette;
	uint8_t r = std::min<uint8_t>(*(paletteData++), 63) * 255 / 63;
	uint8_t g = std::min<uint8_t>(*(paletteData++), 63) * 255 / 63;
	uint8_t b = std::min<uint8_t>(*(paletteData++), 63) * 255 / 63;
	palette.get()[0] = Color(r, g, b, 0);

	// Remaining are solid, so give them 255 alpha.
	std::generate(palette.get().begin() + 1, palette.get().end(),
		[&paletteData]()
	{
		uint8_t r = std::min<uint8_t>(*(paletteData++), 63) * 255 / 63;
		uint8_t g = std::min<uint8_t>(*(paletteData++), 63) * 255 / 63;
		uint8_t b = std::min<uint8_t>(*(paletteData++), 63) * 255 / 63;
		return Color(r, g, b, 255);
	});

	return palette;
}

Palette IMGFile::extractPalette(const std::string &filename)
{
	VFS::IStreamPtr stream = VFS::Manager::get().open(filename);
	DebugAssert(stream != nullptr, "Could not open \"" + filename + "\".");

	stream->seekg(0, std::ios::end);
	std::vector<uint8_t> srcData(stream->tellg());
	stream->seekg(0, std::ios::beg);
	stream->read(reinterpret_cast<char*>(srcData.data()), srcData.size());

	// Read the flags and .IMG file length. Skip the X and Y offsets and dimensions.
	// No need to check for raw override. All given filenames should point to IMGs
	// with "built-in" palettes, and none of those .IMGs are in the raw override.
	const uint16_t flags = Bytes::getLE16(srcData.data() + 8);
	const uint16_t len = Bytes::getLE16(srcData.data() + 10);

	const int headerSize = 12;

	// Don't try to read a built-in palette is there isn't one.
	DebugAssert((flags & 0x0100) != 0, "\"" + filename +
		"\" has no built-in palette to extract.");

	// Get the palette.
	return IMGFile::readPalette(srcData.data() + headerSize + len);
}

int IMGFile::getWidth() const
{
	return this->width;
}

int IMGFile::getHeight() const
{
	return this->height;
}

const Palette *IMGFile::getPalette() const
{
	return this->palette.get();
}

const uint8_t *IMGFile::getPixels() const
{
	return this->pixels.get();
}
