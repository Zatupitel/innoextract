
#include "stream/LzmaFilter.hpp"

#include <stdint.h>

#include <lzma.h>

static lzma_stream * init_raw_lzma_stream(lzma_vli filter, lzma_options_lzma & options) {
	
	options.preset_dict = NULL;
	
	if(options.dict_size > (uint32_t(1) << 28)) {
		throw lzma_error("inno lzma dict size too large", LZMA_FORMAT_ERROR);
	}
	
	lzma_stream * strm = new lzma_stream;
	lzma_stream tmp = LZMA_STREAM_INIT;
	*strm = tmp;
	strm->allocator = NULL;
	
	const lzma_filter filters[2] = { { filter,  &options }, { LZMA_VLI_UNKNOWN } };
	lzma_ret ret = lzma_raw_decoder(strm, filters);
	if(ret != LZMA_OK) {
		delete strm;
		throw lzma_error("inno lzma init error", ret);
	}
	
	return strm;
}

bool lzma_decompressor_impl_base::filter(const char * & begin_in, const char * end_in,
                                         char * & begin_out, char * end_out, bool flush) {
	(void)flush;
	
	lzma_stream * strm = reinterpret_cast<lzma_stream *>(stream);
	
	strm->next_in = reinterpret_cast<const uint8_t *>(begin_in);
	strm->avail_in = end_in - begin_in;
	
	strm->next_out = reinterpret_cast<uint8_t *>(begin_out);
	strm->avail_out = end_out - begin_out;
	
	lzma_ret ret = lzma_code(strm, LZMA_RUN);
	
	begin_in = reinterpret_cast<const char *>(strm->next_in);	
	begin_out = reinterpret_cast<char *>(strm->next_out);
	
	if(ret != LZMA_OK && ret != LZMA_STREAM_END && ret != LZMA_BUF_ERROR) {
		throw lzma_error("lzma decrompression error", ret);
	}
	
	return (ret != LZMA_STREAM_END);
}

void lzma_decompressor_impl_base::close() {
	
	if(stream) {
		lzma_stream * strm = reinterpret_cast<lzma_stream *>(stream);
		lzma_end(strm);
		delete strm, stream = NULL;
	}
}

bool inno_lzma1_decompressor_impl::filter(const char * & begin_in, const char * end_in,
                                          char * & begin_out, char * end_out, bool flush) {
	
	// Decode the header.
	if(!stream) {
		
		// Read enough bytes to decode the header.
		while(nread != 5) {
			if(begin_in == end_in) {
				return true;
			}
			header[nread++] = *begin_in++;
		}
		
		lzma_options_lzma options;
		
		uint8_t properties = header[0];
		if(properties > (9 * 5 * 5)) {
			throw lzma_error("inno lzma1 property error", LZMA_FORMAT_ERROR);
		}
		options.pb = properties / (9 * 5);
		options.lp = (properties % (9 * 5)) / 9;
		options.lc = properties % 9;
		
		options.dict_size = 0;
		for(size_t i = 0; i < 4; i++) {
			options.dict_size += uint32_t(uint8_t(header[i + 1])) << (i * 8);
		}
		
		stream = init_raw_lzma_stream(LZMA_FILTER_LZMA1, options);
	}
	
	return lzma_decompressor_impl_base::filter(begin_in, end_in, begin_out, end_out, flush);
} 

bool inno_lzma2_decompressor_impl::filter(const char * & begin_in, const char * end_in,
                                          char * & begin_out, char * end_out, bool flush) {
	
	// Decode the header.
	if(!stream) {
		
		if(begin_in == end_in) {
			return true;
		}
		
		lzma_options_lzma options;
		
		uint8_t prop = *begin_in++;
		if(prop > 40) {
			throw lzma_error("inno lzma2 property error", LZMA_FORMAT_ERROR);
		}
		
		if(prop == 40) {
			options.dict_size = 0xffffffff;
		} else {
			options.dict_size = (((uint32_t)2 | ((prop) & 1)) << ((prop) / 2 + 11));
		}
		
		stream = init_raw_lzma_stream(LZMA_FILTER_LZMA2, options);
	}
	
	return lzma_decompressor_impl_base::filter(begin_in, end_in, begin_out, end_out, flush);
}
