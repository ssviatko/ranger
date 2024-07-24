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

const std::uint64_t m_guard_size = 16777216;
const std::uint32_t m_cookie = 0xaa5b96dd;
std::array<symbol, 256> m_probs;
std::uint32_t m_message_len;
std::uint64_t m_min_range;
std::uint32_t m_max_count;

void assign_ranges(std::uint64_t a_lo, std::uint64_t a_hi)
{
	std::cout << "characterizing range a_lo " << std::hex << std::setfill('0') << std::setw(16) << a_lo << " a_hi " << a_hi << " size " << (a_hi - a_lo) << std::endl;
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
			long double l_rangetop = (long double)(m_probs[i].count + l_accumulator) / (long double)m_message_len;
			//std::cout << "m_probs[i].count " << std::dec << m_probs[i].count << " l_accumulator " << l_accumulator << " message_len " << m_message_len << " l_rangetop " << std::setprecision(15) << l_rangetop << std::endl;
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

ss::data range_encode(ss::data& a_data, std::size_t a_len)
{
	m_message_len = a_data.size();
	ss::data l_bitstream;
	l_bitstream.set_network_byte_order(true);

	// compute probabilities
	for (std::size_t i = 0; i < 256; ++i)
		m_probs[i] = { 0, 0, 0, 0 }; // init them
	for (std::size_t i = 0; i < a_data.size(); ++i) 
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
	for (std::size_t i = 0; i < m_message_len; ++i) {
		if (i >= a_len)
			break;
		std::cout << "encode loop: position " << i << " read symbol " << std::hex << std::setfill('0') << std::setw(2) << (int)a_data[i] << std::endl;
		l_work_lo.val = m_probs[a_data[i]].lo;
		l_work_hi.val = m_probs[a_data[i]].hi;
		assign_ranges(l_work_lo.val, l_work_hi.val);
		while (l_work_lo.hibyte == l_work_hi.hibyte) {
			std::cout << "--- normalizing... writing " << std::hex << (int)l_work_lo.hibyte << std::endl;
			l_bitstream.write_uint8(l_work_lo.hibyte);
			l_work_lo.val <<= 8;
			l_work_hi.val <<= 8;
			assign_ranges(l_work_lo.val, l_work_hi.val);
		}
	}
	l_bitstream.write_uint64(l_work_lo.val);
	std::cout << "compressed " << std::dec << a_len << " symbols, compressed data len=" << l_bitstream.size() << std::endl;

	// construct full symbol count table
	std::cout << "m_max_count " << std::dec << m_max_count << " bits " << std::bit_width(m_max_count) << std::endl;
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

	// write header
	ss::data l_comp;
	l_comp.set_network_byte_order(true);
	l_comp.write_uint32(m_cookie);
	l_comp.write_uint32(a_len);
	// append frequency table, whichever one is smaller
	std::cout << "cnttbl_full " << std::dec << l_cnttbl_full.size() << " cnttbl_enum " << l_cnttbl_enum.size() << std::endl;
	if (l_cnttbl_full.size() < l_cnttbl_enum.size()) {
		l_comp += l_cnttbl_full;
	} else {
		l_comp += l_cnttbl_enum;
	}
	// append bitstream
	l_comp += l_bitstream;

	std::cout << "full compressed package (with header+count table) " << std::dec << l_comp.size() << " bytes." << std::endl;
	return l_comp;
}

ss::data range_decode(ss::data& a_data)
{
	ss::data l_ret;

	a_data.set_network_byte_order(true); // just to be on the safe side
	std::uint32_t l_cookie = a_data.read_uint32();
	if (l_cookie != m_cookie) {
		// cookie error
		std::cout << "Cookie mismatch" << std::endl;
		exit(EXIT_FAILURE);
	}
	std::uint32_t l_original_size = a_data.read_uint32();
	std::cout << "original size " << std::dec << l_original_size << std::endl;

	// read count table
	ss::data::bit_cursor l_bitcursor;
	l_bitcursor.byte = 8; // step over header data
	a_data.set_read_bit_cursor(l_bitcursor);
	bool l_is_enumerated = a_data.read_bit();
	if (l_is_enumerated) {
		std::cout << "reading enumerated count table..." << std::endl;
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
		std::cout << "reading full count table..." << std::endl;
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
	hidetect l_work;
       	l_work.val = a_data.read_uint64(); // prime the pump
	hidetect l_lo;
	hidetect l_hi;
	l_lo.val = 0;
	l_hi.val = ULLONG_MAX;
	std::cout << "range_decode: starting with work value " << std::hex << std::setfill('0') << std::setw(16) << l_work.val << std::endl;

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
					l_work.val <<= 8;
					l_work.lobyte = a_data.read_uint8();
					//std::cout << "---read from bitstream: " << std::hex << (int)l_work.lobyte << std::endl;
				}
				assign_ranges(l_lo.val, l_hi.val);
				break;
			}
		}
		if (l_pos >= l_original_size)
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

	return l_ret;
}

int main(int argc, char **argv)
{
//	ss::data l_test;
//	l_test.write_std_str("Now is the time for all good men to pull up their pants and stop wanking it to porn. There is no reason for this shit to continue, it is bad for society and has little if any positive benefit.");
//	l_test.write_std_str("here is some more text to work with to pad out the size of our buffer so that we can get a better picture of what's going on here. I want to have some good data to use here as a test vector!");
//	ss::data l_comp = range_encode(l_test);
//	ss::data l_decomp = range_decode(l_comp);
//	std::cout << "l_test    " << l_test.as_hex_str_nospace() << std::endl;
//	std::cout << "l_decomp  " << l_decomp.as_hex_str_nospace() << std::endl;
//	std::cout << "l_test == l_decomp: " << std::boolalpha << (l_test == l_decomp) << std::endl;
	// test with a disk file
	ss::data l_diskfile;
	l_diskfile.load_file("../nbc/motiv-core/main.o");
	ss::data l_diskfile_comp = range_encode(l_diskfile, l_diskfile.size());
	ss::data l_diskfile_decomp = range_decode(l_diskfile_comp);
	std::cout << "diskfile len " << std::dec << l_diskfile.size() << " comp len " << l_diskfile_comp.size() << " decomp len " << l_diskfile_decomp.size() << " ratio " << (double)((double)l_diskfile_comp.size() / (double)l_diskfile.size() * 100.0) << " l_diskfile == l_diskfile_decomp: " << std::boolalpha << (l_diskfile == l_diskfile_decomp) << std::endl;
return 0;
	for (const auto& l_file : std::filesystem::recursive_directory_iterator("..")) {
		if ((l_file.is_regular_file()) && (!(l_file.is_symlink()))) {
			std::cout << "*** attempting to compress: " << l_file.path() << std::endl;
			ss::data l_diskfile;
			l_diskfile.load_file(l_file.path());
			ss::data l_diskfile_comp = range_encode(l_diskfile, l_diskfile.size());
			ss::data l_diskfile_decomp = range_decode(l_diskfile_comp);
			std::cout << "diskfile " << l_file.path() << " len " << std::dec << l_diskfile.size() << " comp len " << l_diskfile_comp.size() << " decomp len " << l_diskfile_decomp.size() << " ratio " << (double)((double)l_diskfile_comp.size() / (double)l_diskfile.size() * 100.0) << " l_diskfile == l_diskfile_decomp: " << std::boolalpha << (l_diskfile == l_diskfile_decomp) << std::endl;
		}
	}

	return 0;
}

