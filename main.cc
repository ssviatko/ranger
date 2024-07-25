#include <iostream>
#include <iomanip>
#include <array>
#include <bit>
#include <filesystem>
#include <cstdint>
#include <climits>

#include "data.h"

union hidetect {
	std::uint64_t val;
	struct {
		std::uint8_t lobyte;
		std::uint8_t mid[6];
		std::uint8_t hibyte;
	};
};

struct symbol {
	std::uint32_t count;
	std::uint64_t lo;
	std::uint64_t hi;
	std::uint64_t size;
};

const std::uint32_t m_cookie = 0xaa5b96dd;
const std::uint32_t m_cookie_multi = 0xaadc5d36;
const std::size_t m_seg_max = 65536;
std::array<symbol, 256> m_probs;
std::uint32_t m_message_len;
std::uint32_t m_segment_len;
std::uint64_t m_min_range;
std::uint32_t m_max_count;

void assign_ranges(std::uint64_t a_lo, std::uint64_t a_hi)
{
	//std::cout << "characterizing range a_lo " << std::hex << std::setfill('0') << std::setw(16) << a_lo << " a_hi " << a_hi << " size " << (a_hi - a_lo) << std::endl;
	if (a_hi == a_lo) {
		std::cout << "can't characterize a zero size range." << std::endl;
		exit(EXIT_FAILURE);
	}
	// assign ranges
	std::uint64_t l_lasthigh = a_lo;
	std::uint64_t l_accumulator = 0;
	m_min_range = ULLONG_MAX;
	for (std::size_t i = 0; i < 256; ++i) {
		if (m_probs[i].count > 0) {
			long double l_rangetop = (long double)(m_probs[i].count + l_accumulator) / (long double)m_segment_len;
			//std::cout << "m_probs[i].count " << std::dec << m_probs[i].count << " l_accumulator " << l_accumulator << " segment_len " << m_segment_len << " l_rangetop " << std::setprecision(15) << l_rangetop << std::endl;
			m_probs[i].lo = l_lasthigh;
			m_probs[i].hi = a_lo + (std::uint64_t)(l_rangetop * (long double)(a_hi - a_lo));
			if ((a_hi == ULLONG_MAX) && (l_rangetop == 1.0)) {
				m_probs[i].hi = ULLONG_MAX;
			}
			l_lasthigh = m_probs[i].hi + 1;
			m_probs[i].size = m_probs[i].hi - m_probs[i].lo;
			m_min_range = std::min(m_probs[i].size, m_min_range);
			//std::cout << "sym " << std::hex << (int)i << " count " << std::dec << (int)m_probs[i].count << " l_rangetop " << std::setprecision(15) << l_rangetop << std::hex << std::setfill('0') << std::setw(16) << " m_probs[i].lo " << m_probs[i].lo << " m_probs[i].hi " << m_probs[i].hi << " l_lasthigh " << l_lasthigh << std::dec << " size " << m_probs[i].size << std::endl;
			l_accumulator += m_probs[i].count;
		}
	}
	//std::cout << "m_min_range " << std::dec << m_min_range << std::endl;
}

ss::data range_encode(ss::data& a_data)
{
	m_message_len = a_data.size();
	std::uint16_t l_seg_count = 1;
	if (m_message_len > m_seg_max) {
		l_seg_count = (m_message_len / m_seg_max);
		if ((m_message_len % m_seg_max) > 0)
			l_seg_count++;
	}
	std::cout << "preparing to compress " << l_seg_count << " segments." << std::endl;
	// write header
	ss::data l_comp;
	l_comp.set_network_byte_order(true);
	if (l_seg_count == 1) {
		l_comp.write_uint32(m_cookie);
	} else {
		l_comp.write_uint32(m_cookie_multi);
		l_comp.write_uint16(l_seg_count);
	}
	l_comp.write_uint64(m_message_len);

	for (std::size_t l_seg = 0; l_seg < l_seg_count; ++l_seg) {
		ss::data l_bitstream;
		l_bitstream.set_network_byte_order(true);
		std::size_t l_segstart = l_seg * m_seg_max;
		std::size_t l_segend = ((l_seg + 1) * m_seg_max);
		if (l_segend > m_message_len)
			l_segend = l_segstart + (m_message_len - (l_seg * m_seg_max));
		m_segment_len = l_segend - l_segstart;
		std::cout << "processing segment " << l_seg + 1 << " segstart " << l_segstart << " segend " << l_segend << std::endl;

		// compute probabilities
		for (std::size_t i = 0; i < 256; ++i)
			m_probs[i] = { 0, 0, 0, 0 }; // init them
//		std::cout << "characterizing probabilities for " << l_segstart << ", " << l_segend << std::endl;
		for (std::size_t i = l_segstart; i < l_segend; ++i) 
			m_probs[a_data[i]].count++;
		// find max symbol count
		m_max_count = 0;
		for (std::size_t i = 0; i < 256; ++i)
			m_max_count = std::max(m_max_count, m_probs[i].count);

		hidetect l_work_lo;
		hidetect l_work_hi;
		l_work_lo.val = 0;
		l_work_hi.val = ULLONG_MAX;
		assign_ranges(l_work_lo.val, l_work_hi.val);
		for (std::size_t i = l_segstart; i < l_segend; ++i) {
//			if ((i % 100000) == 0)
//				std::cout << ".";
			//std::cout << "encode loop: position " << i << " read symbol " << std::hex << std::setfill('0') << std::setw(2) << (int)a_data[i] << std::endl;
			l_work_lo.val = m_probs[a_data[i]].lo;
			l_work_hi.val = m_probs[a_data[i]].hi;
			assign_ranges(l_work_lo.val, l_work_hi.val);
			while (l_work_lo.hibyte == l_work_hi.hibyte) {
				//std::cout << "--- normalizing... writing " << std::hex << (int)l_work_lo.hibyte << std::endl;
				l_bitstream.write_uint8(l_work_lo.hibyte);
				l_work_lo.val <<= 8;
				l_work_hi.val <<= 8;
				l_work_hi.lobyte = 0xff;
				assign_ranges(l_work_lo.val, l_work_hi.val);
			}
		}
		l_bitstream.write_uint64(l_work_lo.val);
//		std::cout << "wrote final quadword " << std::hex << l_work_lo.val << std::endl;
		std::cout << "compressed " << std::dec << (l_segend - l_segstart) << " symbols, compressed data len=" << l_bitstream.size() << std::endl;

		// construct full symbol count table
//		std::cout << "m_max_count " << std::dec << m_max_count << " bits " << std::bit_width(m_max_count) << std::endl;
		ss::data l_cnttbl_full;
		l_cnttbl_full.write_bit(false);
		std::uint64_t l_cntwidth = 0;
		l_cntwidth = std::bit_width(m_max_count);
		l_cnttbl_full.write_bits(l_cntwidth, 5); // 5 bit number from 0-31
		for (std::size_t i = 0; i < 256; ++i) {
			l_cnttbl_full.write_bits(m_probs[i].count, l_cntwidth);
		}

		// construct enumerated count table
		ss::data l_cnttbl_enum;
		l_cnttbl_enum.write_bit(true);
		l_cnttbl_enum.write_bits(l_cntwidth, 5);
		std::uint8_t l_enum_entries = 0;
		for (std::size_t i = 0; i < 256; ++i)
			if (m_probs[i].count > 0)
				l_enum_entries++;
		l_cnttbl_enum.write_bits(l_enum_entries, 8);
		for (std::size_t i = 0; i < 256; ++i) {
			if (m_probs[i].count > 0) {
				l_cnttbl_enum.write_bits(i, 8);
				l_cnttbl_enum.write_bits(m_probs[i].count, l_cntwidth);
			}
		}

		l_comp.write_uint32(l_bitstream.size());
		// append frequency table, whichever one is smaller
//		std::cout << "cnttbl_full " << std::dec << l_cnttbl_full.size() << " cnttbl_enum " << l_cnttbl_enum.size() << std::endl;
		if (l_cnttbl_full.size() < l_cnttbl_enum.size()) {
			l_comp += l_cnttbl_full;
		} else {
			l_comp += l_cnttbl_enum;
		}
		// append bitstream
		l_comp += l_bitstream;
	}

//	std::cout << "full compressed package (with header+count table) " << std::dec << l_comp.size() << " bytes." << std::endl;
	return l_comp;
}

ss::data range_decode(ss::data& a_data)
{
	ss::data l_ret;

	a_data.set_network_byte_order(true); // just to be on the safe side
	std::uint32_t l_cookie = a_data.read_uint32();
	if ((l_cookie != m_cookie) && (l_cookie != m_cookie_multi)) {
		// cookie error
		std::cout << "Cookie mismatch" << std::endl;
		exit(EXIT_FAILURE);
	}
	std::uint16_t l_seg_count = 1;
	if (l_cookie == m_cookie_multi) {
		l_seg_count = a_data.read_uint16();
	}
	std::uint64_t l_original_size = a_data.read_uint64();
//	std::cout << "original size " << std::dec << l_original_size << std::endl;

	for (std::size_t l_seg = 0; l_seg < l_seg_count; ++l_seg) {
		std::size_t l_segstart = l_seg * m_seg_max;
		std::size_t l_segend = ((l_seg + 1) * m_seg_max);
		if (l_segend > l_original_size)
			l_segend = l_segstart + (l_original_size - (l_seg * m_seg_max));
		m_segment_len = l_segend - l_segstart;
		std::uint32_t l_seg_bitstream_size = a_data.read_uint32();
		std::cout << "decoding segment " << l_seg + 1 << " segstart " << l_segstart << " segend " << l_segend << " size " << m_segment_len << " bitstream size " << l_seg_bitstream_size << std::endl;
	
		// read count table
		ss::data::bit_cursor l_bitcursor;
		l_bitcursor.byte = a_data.get_read_cursor(); // step over header data
		a_data.set_read_bit_cursor(l_bitcursor);
		bool l_is_enumerated = a_data.read_bit();
		if (l_is_enumerated) {
//			std::cout << "reading enumerated count table..." << std::endl;
			// read enumerated count table
			std::uint64_t l_cntwidth = a_data.read_bits(5);
			std::uint8_t l_enum_entries = a_data.read_bits(8);
			// clear the table first
			for (std::size_t i = 0; i < 256; ++i) {
				m_probs[i].count = 0;
				m_probs[i].lo = 0;
				m_probs[i].hi = 0;
				m_probs[i].size = 0;
			}
			for (std::size_t i = 0; i < l_enum_entries; ++i) {
				std::uint8_t l_symbol = a_data.read_bits(8);
				std::uint32_t l_frequency = a_data.read_bits(l_cntwidth);
				m_probs[l_symbol].count = l_frequency;
			}
		} else {
//			std::cout << "reading full count table..." << std::endl;
			// read full count table
			std::uint64_t l_cntwidth = a_data.read_bits(5);
			for (std::size_t i = 0; i < 256; ++i) {
				m_probs[i].count = a_data.read_bits(l_cntwidth);
				m_probs[i].lo = 0;
				m_probs[i].hi = 0;
				m_probs[i].size = 0;
			}
		}
		assign_ranges(0, ULLONG_MAX);
		l_bitcursor = a_data.get_read_bit_cursor();
		l_bitcursor.advance_to_next_whole_byte();
		a_data.set_read_bit_cursor(l_bitcursor);
		a_data.set_read_cursor(l_bitcursor.byte);

		// decode
		std::uint32_t l_bitstream_pos = 0;
		hidetect l_work;
       		l_work.val = a_data.read_uint64(); // prime the pump
		l_bitstream_pos += 8;
		hidetect l_lo;
		hidetect l_hi;
		l_lo.val = 0;
		l_hi.val = ULLONG_MAX;
//		std::cout << "range_decode: starting with work value " << std::hex << std::setfill('0') << std::setw(16) << l_work.val << std::endl;

		std::uint32_t l_pos = 0;
		while (1) {
			bool l_found = false;
			for (std::size_t i = 0; i < 256; ++i) {
				if ((l_work.val <= m_probs[i].hi) && (l_work.val >= m_probs[i].lo) && (m_probs[i].count > 0)) {
					//std::cout << "pos " << std::dec << l_pos << " decoded symbol " << std::hex << std::setw(2) << i << " = " << (char)i << std::endl;
					l_ret.write_uint8(i);
					l_pos++;
					l_found = true;
					// if the high byte of the ranges match, scoot the values over and read in next byte from the stream to be the lobyte
					l_lo.val = m_probs[i].lo;
					l_hi.val = m_probs[i].hi;
					assign_ranges(l_lo.val, l_hi.val);
					while (l_lo.hibyte == l_hi.hibyte) {
						l_lo.val <<= 8;
						l_hi.val <<= 8;
						l_hi.lobyte = 0xff;
						l_work.val <<= 8;
						l_work.lobyte = a_data.read_uint8();
						l_bitstream_pos++;
						//std::cout << "---read from bitstream: " << std::hex << (int)l_work.lobyte << " bitstream_pos " << std::dec << l_bitstream_pos << std::endl;
					}
					assign_ranges(l_lo.val, l_hi.val);
					break;
				}
			}
			if (l_pos >= m_segment_len)
				break;
			if (!l_found) {
				// exhausted the for loop... didn't find the range. Fatal error
				std::cout << "unable to find range for work " << std::hex << l_work.val << " at pos " << std::dec << l_pos << std::endl;
				std::cout << "l_lo.val " << std::hex << l_lo.val << " l_hi.val " << l_hi.val << std::endl;
				for (std::size_t i = 0; i < 256; ++i) {
					if (m_probs[i].count > 0) {
						std::cout << "sym " << std::hex << (int)i << " count " << std::dec << std::setfill(' ') << std::setw(0) << (int)m_probs[i].count << std::hex << std::setfill('0') << std::setw(16) << " l_sym.lo " << m_probs[i].lo << " l_sym.hi " << m_probs[i].hi << std::dec << " size " << m_probs[i].size << std::endl;
					}
				}
				exit(EXIT_FAILURE);
			}
		}
		while (l_seg_bitstream_size > l_bitstream_pos) {
			std::uint8_t l_throwaway = a_data.read_uint8();
			std::cout << "throwing away " << std::hex << l_throwaway << " from bitstream" << std::endl;
		}
	}

	return l_ret;
}

int main(int argc, char **argv)
{
	for (const auto& l_file : std::filesystem::recursive_directory_iterator("..")) {
		if ((l_file.is_regular_file()) && (!(l_file.is_symlink()))) {
			std::uintmax_t l_file_size = std::filesystem::file_size(l_file);
			if (l_file_size > 10000000) {
				std::cout << "******** skipping file " << l_file.path() << " because it is " << l_file_size << " bytes in length." << std::endl;
				continue;
			}
			std::cout << "*** attempting to compress: " << l_file.path() << " size " << l_file_size << " loading";
			std::cout.flush();
			ss::data l_diskfile;
			l_diskfile.load_file(l_file.path());
			std::cout << " compressing ";
			std::cout.flush();
			ss::data l_diskfile_comp = range_encode(l_diskfile);
			std::cout << "decompressing";
			std::cout.flush();
			ss::data l_diskfile_decomp = range_decode(l_diskfile_comp);
			bool l_passed = (l_diskfile == l_diskfile_decomp);
			std::cout << std::endl << "diskfile " << l_file.path() << " len " << std::dec << l_diskfile.size() << " comp len " << l_diskfile_comp.size() << " decomp len " << l_diskfile_decomp.size() << " ratio " << (double)((double)l_diskfile_comp.size() / (double)l_diskfile.size() * 100.0) << " l_diskfile == l_diskfile_decomp: " << std::boolalpha << l_passed << std::endl;
			if (!l_passed) {
				std::cout << "File failed integrity check." << std::endl;
				exit(EXIT_FAILURE);
			}
		}
	}

	return 0;
}

