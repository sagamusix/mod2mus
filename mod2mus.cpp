// mod2mus version 0.1
// Convert ProTracker MOD files to Psycho Pinball / Micro Machines 2 MUS format
// 2020 by Saga Musix
// https://sagamusix.de/
// BSD 3-Clause
// Portions of this code are taken from OpenMPT (also BSD 3-Clause)

#define _CRT_SECURE_NO_WARNINGS

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

template<typename T, std::endian endianness>
struct packed
{
	void operator= (T value)
	{
		if (std::endian::native != endianness)
			value = bswap(value);
		std::memcpy(m_value.data(), &value, sizeof(T));
	}

	operator T() const
	{
		T value;
		std::memcpy(&value, m_value.data(), sizeof(T));
		if (std::endian::native != endianness)
			value = bswap(value);
		return value;
	}


private:
	static constexpr uint16_t constexpr_bswap16(uint16_t x) noexcept
	{
		return uint16_t(0)
			| ((x >> 8) & 0x00FFu)
			| ((x << 8) & 0xFF00u)
			;
	}
	static constexpr uint32_t constexpr_bswap32(uint32_t x) noexcept
	{
		return uint32_t(0)
			| ((x & 0x000000FFu) << 24)
			| ((x & 0x0000FF00u) << 8)
			| ((x & 0x00FF0000u) >> 8)
			| ((x & 0xFF000000u) >> 24)
			;
	}

	static constexpr uint32_t bswap(uint32_t x) noexcept { return constexpr_bswap32(x); }
	static constexpr uint32_t bswap(uint16_t x) noexcept { return constexpr_bswap16(x); }
	static constexpr uint32_t bswap(uint8_t x) noexcept { return x; }
	
	static constexpr uint32_t bswap(int32_t x) noexcept { return constexpr_bswap32(x); }
	static constexpr uint32_t bswap(int16_t x) noexcept { return constexpr_bswap16(x); }
	static constexpr uint32_t bswap(int8_t x) noexcept { return x; }

	std::array<std::byte, sizeof(T)> m_value = {};
};

using uint32be = packed<uint32_t, std::endian::big>;
using uint16be = packed<uint16_t, std::endian::big>;
using uint8be = packed<uint8_t, std::endian::big>;
using int32be = packed<int32_t, std::endian::big>;
using int16be = packed<int16_t, std::endian::big>;
using int8be = packed<int8_t, std::endian::big>;

using uint32le = packed<uint32_t, std::endian::little>;
using uint16le = packed<uint16_t, std::endian::little>;
using uint8le = packed<uint8_t, std::endian::little>;
using int32le = packed<int32_t, std::endian::little>;
using int16le = packed<int16_t, std::endian::little>;
using int8le = packed<int8_t, std::endian::little>;


struct MODSampleHeader
{
	std::array<char, 22> name;
	uint16be length;
	uint8be  finetune;
	uint8be  volume;
	uint16be loopStart;
	uint16be loopLength;
};

struct MODFileHeader
{
	std::array<char, 20> name;
	std::array<MODSampleHeader, 31> sample;
	uint8be numOrders;
	uint8be restartPos;
	std::array<uint8be, 128> orderList;
	std::array<char, 4> magic;
};


struct KMSampleHeader
{
	std::array<char, 4> id;
	uint32le chunkSize;  // Chunk size including header

	std::array<char, 32> name;
	uint32le loopStart;
	uint32le sampleSize;
};


struct KMSampleReference
{
	std::array<char, 32> name;
	uint8le finetune;
	uint8le volume;
};


struct KMSongHeader
{
	std::array<char, 4> id;
	uint32le chunkSize;  // Chunk size including header

	std::array<char, 32> name;
	std::array<KMSampleReference, 31> sample;

	uint16le unknown;  // always 0
	uint32le numChannels;
	uint32le restartPos;
	uint32le musicSize;
};


template<typename T>
static void Read(std::istream& s, T& value)
{
	s.read(reinterpret_cast<char *>(&value), sizeof(T));
}

template<typename T>
static void Read(std::istream& s, std::vector<T> &value)
{
	s.read(reinterpret_cast<char*>(value.data()), sizeof(T) * value.size());
}

template<typename T>
static void Write(std::ostream& s, T& value)
{
	s.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
static void Write(std::ostream& s, std::vector<T> &value)
{
	s.write(reinterpret_cast<const char*>(value.data()), sizeof(T) * value.size());
}

static std::array<char, 32> SampleName(int index, const std::array<char, 22> &modName)
{
	std::string s(modName.data(), strnlen(modName.data(), 22));
	for (auto& c : s)
	{
		if(c > 0x00 && c < 0x20)
			c = ' ';
	}
	s = "00:" + s;
	s[0] += index / 10;
	s[1] += index % 10;
	s.resize(32);
	std::array<char, 32> a;
	std::copy(s.begin(), s.end(), a.begin());
	return a;
}


static constexpr std::array<uint16_t, 3 * 12> ProTrackerPeriodTable =
{
	856,808,762,720,678,640,604,570,538,508,480,453,
	428,404,381,360,339,320,302,285,269,254,240,226,
	214,202,190,180,170,160,151,143,135,127,120,113,
};


int main(const int argc, const char *argv[])
{
	if (argc != 3)
	{
		std::cout << "mod2mus version 0.1 by Saga Musix" << std::endl;
		std::cout << "syntax: " << argv[0] << " infile.mod outfile.mus" << std::endl;
		return 0;
	}

	std::ifstream fIn(argv[1], std::ios::binary);
	if (!fIn)
	{
		std::cout << "error: cannot open input file" << std::endl;
		return -1;
	}
	std::ofstream fOut(argv[2], std::ios::binary);
	if (!fOut)
	{
		std::cout << "error: cannot open output file" << std::endl;
		return -2;
	}

	MODFileHeader modHeader;
	Read(fIn, modHeader);

	std::array<const char *, 4> magic = {"1CHN", "2CHN", "3CHN", "M.K."};
	int numChannels = 0;
	for(int i = 0; i < 4; i++)
	{
		if(!std::memcmp(magic[i], modHeader.magic.data(), 4))
		{
			numChannels = i + 1;
			break;
		}
	}
	if(!numChannels)
	{
		std::cout << "error: cannot identify input file (MOD signature is not 1CHN, 2CHN, 3CHN or M.K.)" << std::endl;
		return -3;
	}

	if(modHeader.numOrders > 128)
	{
		std::cout << "error: input file is malformed (claims > 128 orders)" << std::endl;
		return -4;
	}

	int numPatterns = 0;
	for (int ord = 0; ord < 128; ord++)
	{
		int pat = modHeader.orderList[ord];
		if (pat < 128 && numPatterns <= pat)
		{
			numPatterns = pat + 1;
		}
	}

	KMSongHeader kmSong{};
	std::memcpy(kmSong.id.data(), "SONG", 4);
	std::strncpy(kmSong.name.data(), modHeader.name.data(), std::size(modHeader.name));

	if (!strlen(kmSong.name.data()))
	{
		std::cout << "warning: song name is empty" << std::endl;
	}

	for (auto& c : kmSong.name)
	{
		if (c > 0x00 && c < 0x20)
			c = ' ';
	}

	for (int i = 0; i < 31; i++)
	{
		const MODSampleHeader& modSample = modHeader.sample[i];
		KMSampleReference& kmSample = kmSong.sample[i];

		if(modSample.length < 2)
			continue;
		
		kmSample.name = SampleName(i + 1, modSample.name);
		kmSample.finetune = modSample.finetune & 0x0F;
		if(modSample.volume < 64)
			kmSample.volume = modSample.volume;
		else
			kmSample.volume = 64;
	}

	kmSong.numChannels = numChannels;

	struct ChannelState
	{
		uint8_t note = 0xFF;
		uint8_t instr = 0xFF;
		uint8_t command = 0xFF;
		uint8_t param = 0xFF;
		size_t repeatOffset = 0;
	};

	std::vector<ChannelState> channelState(numChannels);
	std::vector<uint8_t> musicData;

	int startRow = 0;
	for (int ord = 0; ord < modHeader.numOrders; ord++)
	{
		fIn.seekg(1084 + (numChannels * 64 * 4) * modHeader.orderList[ord], std::ios::beg);

		if(modHeader.restartPos == ord)
			kmSong.restartPos = static_cast<uint32_t>(musicData.size());

		for (int row = startRow; row < 64; row++)
		{
			startRow = 0;
			for (int chn = 0; chn < numChannels; chn++)
			{
				auto &chnState = channelState[chn];

				std::array<uint8_t, 4> modData;
				Read(fIn, modData);

				uint16_t period = ((static_cast<uint16_t>(modData[0]) & 0x0F) << 8) | modData[1];
				uint8_t note = 0;
				if (period > 0 && period != 0xFFF)
				{
					for (uint8_t i = 0; i < static_cast<uint8_t>(std::size(ProTrackerPeriodTable)); i++)
					{
						if (period >= ProTrackerPeriodTable[i])
						{
							if (period != ProTrackerPeriodTable[i] && i != 0)
							{
								uint16_t p1 = ProTrackerPeriodTable[i - 1];
								uint16_t p2 = ProTrackerPeriodTable[i];
								if (p1 - period < (period - p2))
								{
									note = i;
									break;
								}
							}
							note = i + 1;
							break;
						}
					}
				}
				uint8_t instr = (modData[2] >> 4) | (modData[0] & 0x10);
				uint8_t command = 0x014;
				uint8_t param = modData[3];

				switch (modData[2] & 0x0F)
				{
				case 0x00: if(param) command = 0x0B; break;
				case 0x01: command = 0x0C; break;
				case 0x02: command = 0x0D; break;
				case 0x03: command = 0x07; break;
				case 0x04: command = 0x09; break;
				case 0x05: command = 0x08; break;
				case 0x06: command = 0x0A; break;
				case 0x07: command = 0x10; break;
				case 0x09: command = 0x06; break;
				case 0x0A: command = 0x0E; break;
				case 0x0B:
					if(param > ord)
						ord = param - 1;
					param = 0x00;
					startRow = 0;
					row = 64;
					break;
				case 0x0C: command = 0x00; break;
				case 0x0D:
					param = 0x00;
					startRow = param;
					row = 64;
					break;
				case 0x0E:
					param &= 0x0F;
					switch (modData[3] & 0xF0)
					{
					case 0x10: command = 0x03; break;
					case 0x20: command = 0x04; break;
					case 0x90: command = 0x0F; break;
					case 0xA0: command = 0x01; break;
					case 0xB0: command = 0x02; break;
					case 0xC0: command = 0x11; break;
					default: param = 0x00; break;
					}
					break;
				case 0x0F: command = 0x12; break;
				default:
					param = 0;
					break;
				}

				if (note == chnState.note && instr == chnState.instr && command == chnState.command && param == chnState.param)
				{
					if (chnState.repeatOffset && musicData[chnState.repeatOffset] < 0xFF)
					{
						musicData[chnState.repeatOffset]++;
					}
					else
					{
						chnState.repeatOffset = musicData.size();
						musicData.push_back(0x80);
					}
					continue;
				}
				chnState.repeatOffset = 0;

				if (command == chnState.command && param == chnState.param)
				{
					musicData.back() |= 0x80;
					continue;
				}

				musicData.push_back(chnState.note = note);
				musicData.push_back(chnState.instr = instr);
				musicData.push_back(chnState.command = command);
				musicData.push_back(chnState.param = param);
			}
		}
	}

	kmSong.chunkSize = static_cast<uint32_t>(sizeof(kmSong) + musicData.size());
	kmSong.musicSize = static_cast<uint32_t>(musicData.size());

	Write(fOut, kmSong);
	Write(fOut, musicData);

	fIn.seekg(1084 + (numChannels * 64 * 4) * numPatterns, std::ios::beg);

	for(int i = 0; i < 31; i++)
	{
		const MODSampleHeader &modSample = modHeader.sample[i];

		KMSampleHeader kmSample{};
		std::memcpy(kmSample.id.data(), "SMPL", 4);
		kmSample.name = SampleName(i + 1, modSample.name);
		if(modSample.loopLength > 1 && modSample.loopStart < modSample.length && modSample.loopStart + modSample.loopLength + modSample.length)
		{
			kmSample.loopStart = modSample.loopStart * 2;
			kmSample.sampleSize = kmSample.loopStart + modSample.loopLength * 2;
		} else
		{
			kmSample.sampleSize = modSample.length * 2;
			kmSample.loopStart = kmSample.sampleSize;
		}
		kmSample.chunkSize = sizeof(kmSample) + kmSample.sampleSize;

		std::vector<char> sampleData(kmSample.sampleSize);
		Read(fIn, sampleData);

		if (modSample.length < 2)
			continue;

		Write(fOut, kmSample);
		Write(fOut, sampleData);
	}

	return 0;
}
