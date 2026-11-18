#include "Inflate.h"

#include <array>

// Minimal zlib/DEFLATE decoder. Compressed plugin records are the only consumer:
// we need the decompressed record bytes, not the whole zlib surface, so this is
// a self-contained RFC 1950/1951 reader instead of a new build dependency.

namespace IIF::ReloadTime
{
	namespace
	{
		constexpr int kMaxBits = 15;
		constexpr int kMaxLCodes = 286;
		constexpr int kMaxDCodes = 30;
		constexpr int kFixLCodes = 288;

		struct BitReader
		{
			const std::uint8_t* data{ nullptr };
			std::size_t size{ 0 };
			std::size_t pos{ 0 };
			std::uint32_t bitbuf{ 0 };
			int bitcnt{ 0 };

			// Returns the next a_need bits, or -1 when the input is exhausted.
			int Bits(int a_need)
			{
				std::uint32_t val = bitbuf;
				while (bitcnt < a_need) {
					if (pos >= size) return -1;
					val |= static_cast<std::uint32_t>(data[pos++]) << bitcnt;
					bitcnt += 8;
				}
				bitbuf = val >> a_need;
				bitcnt -= a_need;
				return static_cast<int>(val & ((1u << a_need) - 1));
			}
		};

		struct Huffman
		{
			std::array<std::uint16_t, kMaxBits + 1> count{};
			std::array<std::uint16_t, kFixLCodes> symbol{};
		};

		int Decode(BitReader& a_in, const Huffman& a_huff)
		{
			int code = 0;
			int first = 0;
			int index = 0;
			for (int len = 1; len <= kMaxBits; ++len) {
				const int bit = a_in.Bits(1);
				if (bit < 0) return -1;
				code |= bit;
				const int count = a_huff.count[len];
				if (code - first < count) return a_huff.symbol[index + (code - first)];
				index += count;
				first = (first + count) << 1;
				code <<= 1;
			}
			return -1;
		}

		bool Construct(Huffman& a_huff, const std::uint8_t* a_lengths, int a_n)
		{
			a_huff.count.fill(0);
			for (int i = 0; i < a_n; ++i) ++a_huff.count[a_lengths[i]];
			if (a_huff.count[0] == a_n) return true;  // complete code with no symbols

			int left = 1;
			for (int len = 1; len <= kMaxBits; ++len) {
				left <<= 1;
				left -= a_huff.count[len];
				if (left < 0) return false;  // over-subscribed
			}

			std::array<std::uint16_t, kMaxBits + 1> offs{};
			for (int len = 1; len < kMaxBits; ++len) offs[len + 1] = static_cast<std::uint16_t>(offs[len] + a_huff.count[len]);
			for (int symbol = 0; symbol < a_n; ++symbol) {
				if (a_lengths[symbol] != 0) a_huff.symbol[offs[a_lengths[symbol]]++] = static_cast<std::uint16_t>(symbol);
			}
			return true;
		}

		bool Stored(BitReader& a_in, std::vector<std::uint8_t>& a_out, std::size_t a_limit)
		{
			a_in.bitbuf = 0;
			a_in.bitcnt = 0;  // stored blocks start on the next byte boundary
			if (a_in.pos + 4 > a_in.size) return false;

			const std::uint32_t len = static_cast<std::uint32_t>(a_in.data[a_in.pos]) |
			                          (static_cast<std::uint32_t>(a_in.data[a_in.pos + 1]) << 8);
			const std::uint32_t nlen = static_cast<std::uint32_t>(a_in.data[a_in.pos + 2]) |
			                           (static_cast<std::uint32_t>(a_in.data[a_in.pos + 3]) << 8);
			if (len != (~nlen & 0xFFFFu)) return false;
			a_in.pos += 4;

			if (a_in.pos + len > a_in.size) return false;
			if (a_out.size() + len > a_limit) return false;
			a_out.insert(a_out.end(), a_in.data + a_in.pos, a_in.data + a_in.pos + len);
			a_in.pos += len;
			return true;
		}

		constexpr std::array<int, 29> kLenBase{ 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
			35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
		constexpr std::array<int, 29> kLenBits{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
			3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
		constexpr std::array<int, 30> kDistBase{ 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
			257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
		constexpr std::array<int, 30> kDistBits{ 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
			7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

		bool Codes(BitReader& a_in, const Huffman& a_lencode, const Huffman& a_distcode, std::vector<std::uint8_t>& a_out, std::size_t a_limit)
		{
			for (;;) {
				int symbol = Decode(a_in, a_lencode);
				if (symbol < 0) return false;
				if (symbol < 256) {
					if (a_out.size() >= a_limit) return false;
					a_out.push_back(static_cast<std::uint8_t>(symbol));
				}
				else if (symbol > 256) {
					symbol -= 257;
					if (symbol >= 29) return false;

					const int lenExtra = a_in.Bits(kLenBits[symbol]);
					if (lenExtra < 0) return false;
					const int length = kLenBase[symbol] + lenExtra;

					const int distSymbol = Decode(a_in, a_distcode);
					if (distSymbol < 0 || distSymbol >= kMaxDCodes) return false;
					const int distExtra = a_in.Bits(kDistBits[distSymbol]);
					if (distExtra < 0) return false;
					const std::size_t distance = static_cast<std::size_t>(kDistBase[distSymbol] + distExtra);

					if (distance > a_out.size()) return false;
					if (a_out.size() + static_cast<std::size_t>(length) > a_limit) return false;
					for (int i = 0; i < length; ++i) {
						a_out.push_back(a_out[a_out.size() - distance]);
					}
				}
				else {
					return true;  // end of block
				}
			}
		}

		void Fixed(Huffman& a_lencode, Huffman& a_distcode)
		{
			std::array<std::uint8_t, kFixLCodes> lengths{};
			int symbol = 0;
			for (; symbol < 144; ++symbol) lengths[symbol] = 8;
			for (; symbol < 256; ++symbol) lengths[symbol] = 9;
			for (; symbol < 280; ++symbol) lengths[symbol] = 7;
			for (; symbol < kFixLCodes; ++symbol) lengths[symbol] = 8;
			Construct(a_lencode, lengths.data(), kFixLCodes);

			for (symbol = 0; symbol < kMaxDCodes; ++symbol) lengths[symbol] = 5;
			Construct(a_distcode, lengths.data(), kMaxDCodes);
		}

		bool Dynamic(BitReader& a_in, std::vector<std::uint8_t>& a_out, std::size_t a_limit)
		{
			const int nlen = a_in.Bits(5);
			const int ndist = a_in.Bits(5);
			const int ncode = a_in.Bits(4);
			if (nlen < 0 || ndist < 0 || ncode < 0) return false;
			const int nlenCodes = nlen + 257;
			const int ndistCodes = ndist + 1;
			const int ncodeCodes = ncode + 4;
			if (nlenCodes > kMaxLCodes || ndistCodes > kMaxDCodes) return false;

			constexpr std::array<int, 19> kOrder{ 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
			std::array<std::uint8_t, kMaxLCodes + kMaxDCodes> lengths{};
			for (int i = 0; i < ncodeCodes; ++i) {
				const int len = a_in.Bits(3);
				if (len < 0) return false;
				lengths[kOrder[i]] = static_cast<std::uint8_t>(len);
			}

			Huffman lencode{};
			Huffman distcode{};
			if (!Construct(lencode, lengths.data(), 19)) return false;

			int index = 0;
			while (index < nlenCodes + ndistCodes) {
				int symbol = Decode(a_in, lencode);
				if (symbol < 0) return false;
				if (symbol < 16) {
					lengths[index++] = static_cast<std::uint8_t>(symbol);
					continue;
				}

				std::uint8_t repeat = 0;
				int count = 0;
				if (symbol == 16) {
					if (index == 0) return false;
					repeat = lengths[index - 1];
					const int extra = a_in.Bits(2);
					if (extra < 0) return false;
					count = 3 + extra;
				}
				else if (symbol == 17) {
					const int extra = a_in.Bits(3);
					if (extra < 0) return false;
					count = 3 + extra;
				}
				else {
					const int extra = a_in.Bits(7);
					if (extra < 0) return false;
					count = 11 + extra;
				}
				if (index + count > nlenCodes + ndistCodes) return false;
				while (count-- > 0) lengths[index++] = repeat;
			}

			if (lengths[256] == 0) return false;  // no end-of-block code
			if (!Construct(lencode, lengths.data(), nlenCodes)) return false;
			if (!Construct(distcode, lengths.data() + nlenCodes, ndistCodes)) return false;
			return Codes(a_in, lencode, distcode, a_out, a_limit);
		}
	}

	bool ZlibDecompressCapped(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_cap, std::vector<std::uint8_t>& a_out)
	{
		a_out.clear();
		if (!a_data || a_size < 6 || a_cap == 0) return false;

		// zlib wrapper: CM must be deflate, header must be a multiple of 31, no preset dictionary.
		if ((a_data[0] & 0x0F) != 8) return false;
		if (((static_cast<std::uint32_t>(a_data[0]) << 8) | a_data[1]) % 31 != 0) return false;
		if ((a_data[1] & 0x20) != 0) return false;

		BitReader in{ a_data + 2, a_size - 2 };
		std::vector<std::uint8_t> out;
		out.reserve(a_cap < (1u << 20) ? a_cap : (1u << 20));

		bool last = false;
		while (!last) {
			const int lastBit = in.Bits(1);
			const int type = in.Bits(2);
			if (lastBit < 0 || type < 0) return false;
			last = lastBit != 0;

			bool ok = false;
			switch (type) {
			case 0:
				ok = Stored(in, out, a_cap);
				break;
			case 1:
				{
					Huffman lencode{};
					Huffman distcode{};
					Fixed(lencode, distcode);
					ok = Codes(in, lencode, distcode, out, a_cap);
				}
				break;
			case 2:
				ok = Dynamic(in, out, a_cap);
				break;
			default:
				ok = false;
				break;
			}
			if (!ok) return false;
		}

		a_out = std::move(out);
		return true;
	}

	bool ZlibDecompress(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_expectedSize, std::vector<std::uint8_t>& a_out)
	{
		std::vector<std::uint8_t> out;
		if (!ZlibDecompressCapped(a_data, a_size, a_expectedSize, out)) return false;
		if (out.size() != a_expectedSize) return false;
		a_out = std::move(out);
		return true;
	}
}
