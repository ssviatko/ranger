#ifndef SS2XDATA_H
#define SS2XDATA_H

#include <iostream>
#include <iomanip>
#include <format>
#include <string>
#include <sstream>
#include <sstream>
#include <fstream>
#include <vector>
#include <deque>
#include <map>
#include <stack>
#include <exception>
#include <bit>
#include <algorithm>
#include <random>
#include <optional>

#include <cstdint>

#include <memory.h>
#include <stdint.h>

#include "bf.h"
#include "md5.h"
#include "sha1.h"
#include "sha2.h"

namespace ss {

class data_exception : public std::exception {
public:
	data_exception(const char* a_what) throw();
	data_exception(const std::string& a_what) throw();
	~data_exception() throw() { } // Destroy exception object.
	virtual const char* what() const throw();
	
protected:
	std::string m_what; // Error string.
};

class data {

	const static std::uint32_t crc32_tab[];
	const static std::uint8_t byte_mask[];

	typedef union {
		std::uint8_t uint8_val;
		std::int8_t int8_val;
	} size1_union;

	typedef union {
		std::uint16_t uint16_val;
		std::int16_t int16_val;
		std::uint8_t raw[2];
	} size2_union;

	typedef union {
		std::uint32_t uint32_val;
		std::int32_t int32_val;
		float float_val;
		std::uint8_t raw[4];
	} size4_union;

	typedef union {
		std::uint64_t uint64_val;
		std::int64_t int64_val;
		double double_val;
		std::uint8_t raw[8];
	} size8_union;
	
	typedef union {
		long double longdouble_val;
		std::uint8_t raw[16];
	} size16_union;

	std::vector<std::uint8_t> read_raw_data(std::size_t a_num_bytes);
	void write_raw_data(const std::vector<std::uint8_t>& a_vector);
	
	//static private utility methods for textual presentation and initialization
	static std::string hex_str(const std::uint8_t *a_data, std::size_t a_len);
	static std::uint8_t *hex_decode(const std::string a_str, std::size_t *decode_len);
	static std::string base64_str(const std::uint8_t *a_data, std::size_t a_len);
	static std::uint8_t *base64_decode(const std::string a_str, std::size_t *decode_len);
	
	void copy_construct(const data& a_data);

public:

	class bit_cursor {
	public:
		bit_cursor() : byte(0), bit(7) { }
		// bits start at bit 7 and count down to 0
		std::uint64_t byte;
		std::uint16_t bit;
		void set_absolute(std::uint64_t a_absolute); // lower 64 bits of absolute bit position
		void advance_to_next_whole_byte() { if (bit < 7) { ++byte; bit = 7; } }
		std::uint64_t get_absolute();
	};

	/* huffman related */

	class huff_tree_node {
	public:
		enum { LEAF, INTERNAL };
		huff_tree_node() : type(LEAF), id(0), symbol(0), freq(0), left_id(-1), right_id(-1) { }
		std::int16_t type;
		std::int16_t id;
		std::uint8_t symbol;
		std::uint64_t freq;
		std::int16_t left_id;
		std::int16_t right_id;
		bool operator<(const huff_tree_node& rhs) const { return (freq < rhs.freq); }
	};

	const static uint32_t HUFF_MAGIC_COOKIE = 0xc0edbabe;

	/* constructors */
	
	data();
	data(const data& a_data);
	data(data&& a_data);
	~data();
	
	void dump_hex() const;
	std::string as_hex_str() const;
	std::string as_hex_str_nospace() const;

	/* files */
	
	void save_file(const std::string& a_filename);
	void load_file(const std::string& a_filename);

	/* cursor management/settings */
	
	void set_write_cursor(std::size_t a_write_cursor);
	void set_write_cursor_to_append();
	void set_read_cursor(std::size_t a_read_cursor);
	void set_network_byte_order(bool a_setting) { m_network_byte_order = a_setting; };
	
	std::size_t get_write_cursor() { return m_write_cursor; };
	std::size_t get_read_cursor() { return m_read_cursor; };
	bool get_network_byte_order() { return m_network_byte_order; };
	std::size_t size() const;

	/* utilities */
	
	void fill(std::size_t a_num_bytes, std::uint8_t a_val);
	void random(std::size_t a_num_bytes);
	void clear();
	void truncate_back(std::size_t a_new_len);
	void assign(std::uint8_t *a_buffer, std::size_t a_len);
	bool compare(const data& a_data) const; // true = same, false = different
	void append_data(const data& a_data);
	
	/* operators */
	
	bool operator==(const data& rhs) const;
	bool operator!=(const data& rhs) const;
	std::strong_ordering operator<=>(const data& rhs) const;
	data& operator=(const data& a_data);
	data& operator=(data&& a_data);
	data& operator+=(const data& a_data);
	std::uint8_t& operator[](std::size_t index);

	/* bits */
	
	void set_read_bit_cursor(bit_cursor a_bit_cursor);
	void set_write_bit_cursor(bit_cursor a_bit_cursor);
	void set_write_bit_cursor_to_append(); // write cursor at bit 7, m_buffer_datalen + 1
	bit_cursor get_read_bit_cursor() const { return m_read_bit_cursor; }
	bit_cursor get_write_bit_cursor() const { return m_write_bit_cursor; }
	void write_true(); // write 1 bit at bit cursor, value 1 (true)
	void write_false(); // write 1 bit at bit cursor, value 0 (false);
	void write_bit(bool a_bit);
	void write_bit(std::uint64_t a_bit); // integer version: 0=false, >0 = true, 
	void write_bits(std::uint64_t a_bits, std::uint16_t a_count);
	bool read_bit();
	std::uint64_t read_bits(std::uint16_t a_count);
	std::string as_bits();
	void from_bits(const std::string& a_str);

	/* basic C/C++ inbuilt types */
	
	void write_uint8(std::uint8_t a_uint8);
	std::uint8_t read_uint8();
	void write_int8(std::int8_t a_int8);
	std::int8_t read_int8();

	void write_uint16(std::uint16_t a_uint16);
	std::uint16_t read_uint16();
	void write_int16(std::int16_t a_int16);
	std::int16_t read_int16();
	
	void write_uint32(std::uint32_t a_uint32);
	std::uint32_t read_uint32();
	void write_int32(std::int32_t a_int32);
	std::int32_t read_int32();

	void write_uint64(std::uint64_t a_uint64);
	std::uint64_t read_uint64();
	void write_int64(std::int64_t a_int64);
	std::int64_t read_int64();

	void write_float(float a_float);
	float read_float();
	void write_double(double a_double);
	double read_double();
	// note: platform-specific 16 byte double, ignores endianness setting
	void write_longdouble(long double a_longdouble);
	long double read_longdouble();
	
	/* strings */
	
	void set_delimiter(std::uint8_t a_delimiter) { m_delimiter = a_delimiter; };
	std::uint8_t get_delimiter() { return m_delimiter; };
	
	void write_std_str(const std::string& a_str);
	std::string read_std_str(std::size_t a_length);
	void write_std_str_delim(const std::string& a_str);
	std::optional<std::string> read_std_str_delim();
	
	/* textual presentation and initialization */
	
	void write_hex_str(const std::string& a_str);
	std::string read_hex_str(std::size_t a_len);
	void write_base64(const std::string& a_str);
	std::string read_base64(std::size_t a_len);
	std::string as_base64();
	void from_base64(const std::string& a_str);

	/* hashing */
	
	std::uint32_t crc32(std::uint32_t a_crc);
	data md5();
	data sha1();
	data sha2_224();
	data sha2_256();
	data sha2_384();
	data sha2_512();
	
	/* encryption */
	
	static data bf_key_random();
	static data bf_key_schedule(const std::string& a_string);
	static data bf_block_encrypt(data& a_block, data& a_key);
	static data bf_block_decrypt(data& a_block, data& a_key);
	
	/* compression */
	
	data huffman_encode() const;
	data huffman_decode() const;
	void set_huffman_debug(bool a_debug) { m_huffman_debug = a_debug; };
	data rle_encode() const;
	data rle_decode() const;
	
protected:
	bool m_network_byte_order;
	std::size_t m_read_cursor;
	std::size_t m_write_cursor;
	bit_cursor m_read_bit_cursor;
	bit_cursor m_write_bit_cursor;
	std::vector<std::uint8_t> m_buffer;
	std::uint8_t m_delimiter;
	bool m_huffman_debug;
};

}; // namespace ss

#endif // SS2XDATA_H

