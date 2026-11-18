#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace IIF::ReloadTime
{
	// Decompresses a zlib stream (RFC 1950: 2-byte header + DEFLATE + Adler-32).
	// a_out receives exactly a_expectedSize bytes on success.
	// Returns false for malformed input or a decompressed size mismatch.
	bool ZlibDecompress(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_expectedSize, std::vector<std::uint8_t>& a_out);

	// Same decoder without a known decompressed size: stops at the end-of-stream
	// marker and never exceeds a_cap bytes. Used when a resource hands back the
	// stored (zlib-compressed) archive entry instead of the decompressed file.
	bool ZlibDecompressCapped(const std::uint8_t* a_data, std::size_t a_size, std::size_t a_cap, std::vector<std::uint8_t>& a_out);
}
