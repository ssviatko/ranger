#include <iostream>
#include <iomanip>
#include <map>
#include <cstdint>
#include <climits>

#include "data.h"

struct symbol {
	std::uint8_t count;
	std::uint64_t lo;
	std::uint64_t hi;
	std::uint64_t size;
};

std::map<std::uint8_t, symbol> probs;
std::uint16_t message_len;
std::uint64_t min_range;

void assign_ranges(std::uint64_t a_lo, std::uint64_t a_hi)
{
	std::cout << "characterizing range a_lo " << std::hex << std::setfill('0') << std::setw(16) << a_lo << " a_hi " << a_hi << " size " << (a_hi - a_lo) << std::endl;
	// assign ranges
	std::uint64_t l_lasthigh = a_lo;
	std::uint16_t l_accumulator = 0;
	min_range = ULLONG_MAX;
	for (std::size_t i = 0; i < 256; ++i) {
		symbol l_sym = probs[i];
		if (l_sym.count > 0) {
			double l_rangetop = (double)(l_sym.count + l_accumulator) / (double)message_len;
			l_sym.lo = l_lasthigh;
			l_sym.hi = a_lo + (std::uint64_t)(l_rangetop * (double)(a_hi - a_lo));
			l_lasthigh = l_sym.hi + 1;
			l_sym.size = l_sym.hi - l_sym.lo;
			min_range = std::min(l_sym.size, min_range);
			//std::cout << "sym " << std::hex << (int)i << " count " << (int)l_sym.count << " l_rangetop " << l_rangetop << std::hex << std::setfill('0') << std::setw(16) << " l_sym.lo " << l_sym.lo << " l_sym.hi " << l_sym.hi << " l_lasthigh " << l_lasthigh << std::dec << " size " << l_sym.size << std::endl;
			l_accumulator += l_sym.count;
			probs[i] = l_sym;
		}
	}
}

ss::data range_encode(ss::data& a)
{
	message_len = a.size();
	ss::data l_bitstream;

	// compute probabilities
	for (std::size_t i = 0; i < 256; ++i)
		probs[i] = { 0, 0, 0 }; // init them
	for (std::size_t i = 0; i < a.size(); ++i) {
		symbol l_sym = probs[a[i]];
		l_sym.count++;
		probs[a[i]] = l_sym;
	}

	std::uint64_t work_lo = 0;
	std::uint64_t work_hi = ULLONG_MAX;
	assign_ranges(work_lo, work_hi);
	for (std::size_t i = 0; i < message_len; ++i) {
		if (min_range < 4096) {
			std::cout << "finished work_lo " << std::hex << work_lo << " work_hi " << work_hi << std::endl;
			std::uint16_t bits = 0;
			std::cout << "writing bits: ";
			while (1) {
				bool bit_lo = ((work_lo & 0x8000000000000000ULL) > 0);
				bool bit_hi = ((work_hi & 0x8000000000000000ULL) > 0);
				if (bit_lo == bit_hi) {
					std::cout << bit_lo;
					l_bitstream.write_bit(bit_lo);
					work_lo <<= 1;
					work_hi <<= 1;
					bits++;
				} else {
					break;
				}
			}
			std::cout << std::endl << "wrote " << std::dec << bits << " bits." << std::endl;
			std::cout << "new finished work_lo " << std::hex << work_lo << " work_hi " << work_hi << std::endl;
			assign_ranges(work_lo, work_hi);
		}
		symbol l_sym = probs[a[i]];
		std::cout << "encode loop: position " << i << " read symbol " << std::hex << std::setfill('0') << std::setw(2) << (int)a[i] << std::endl;
		work_lo = l_sym.lo;
		work_hi = l_sym.hi;
		assign_ranges(work_lo, work_hi);
	}
	std::cout << "final finished work_lo " << std::hex << work_lo << " work_hi " << work_hi << std::endl;
	std::uint64_t l_midpoint = (work_hi - ((work_hi - work_lo) / 2));
	std::cout << "midpoint: " << std::hex << l_midpoint << std::endl;
	std::cout << "take the midpoint of these and write out these 64 bits and tie a ribbon on it." << std::endl;
	l_bitstream.write_bits(l_midpoint, 64);
	std::cout << "l_bitstream = " << l_bitstream.as_hex_str() << std::endl;
	std::cout << "l_bitstream len " << std::dec << l_bitstream.size() << " original len " << message_len << std::endl;
	return l_bitstream;
}

void range_decode(ss::data& a)
{
	// assume our probability table is already constructed. in reality we will load it from the file with the compressed data
	assign_ranges(0, ULLONG_MAX);
	ss::data::bit_cursor l_bitcursor;
	a.set_read_bit_cursor(l_bitcursor);
	std::uint64_t work = a.read_bits(64); // prime the pump
	std::cout << "range_decode: starting with work value " << std::hex << std::setfill('0') << std::setw(16) << work << std::endl;
	do {
		// find range work value belongs to
		for (std::size_t i = 0; i < 256; ++i) {
			symbol l_sym = probs[i];
			if ((work < l_sym.hi) && (work > l_sym.lo)) {
				std::cout << "decoded symbol " << std::hex << std::setw(2) << i << " = " << (char)i << std::endl;
				assign_ranges(l_sym.lo, l_sym.hi);
				break;
			}
		}
	} while (min_range > 4095);
}

int main(int argc, char **argv)
{
	ss::data l_test;
	l_test.write_std_str("Now is the time for all good men to pull up their pants and stop wanking it to porn. There is no reason for this shit to continue, it is bad for society and has little if any positive benefit.");
	l_test.write_std_str("here is some more text to work with to pad out the size of our buffer so that we can get a better picture of what's going on here. I want to have some good data to use here as a test vector!");
	ss::data l_comp = range_encode(l_test);
	range_decode(l_comp);

	return 0;
}

