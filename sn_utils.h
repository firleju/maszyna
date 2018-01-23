/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <CpperoMQ\IncomingMessage.hpp>

class sn_utils
{
public:
	static uint16_t ld_uint16(std::istream&);
	static uint32_t ld_uint32(std::istream&);
	static int32_t ld_int32(std::istream&);
	static int32_t ld_int32(std::vector<uint8_t>&);
	static float ld_float32(std::istream&);
	static float ld_float32(std::vector<uint8_t>&);
	static double ld_float64(std::istream&);
	static std::string d_str(std::istream&);
    static bool d_bool(std::istream&);
    static glm::dvec3 d_dvec3(std::istream&);
    static glm::vec4 d_vec4(std::istream&);

	static void ls_uint16(std::ostream&, uint16_t);
	static void ls_uint32(std::ostream&, uint32_t);
	static void ls_int32(std::ostream&, int32_t);
	static void ls_int32(std::vector<uint8_t>&, int32_t);
	static void ls_float32(std::ostream&, float);
	static void ls_float32(std::vector<uint8_t>&, float);
	static void ls_float64(std::ostream&, double);
	static void ls_float64(std::vector<uint8_t>&, double);
	static void s_str(std::ostream&, std::string);
    static void s_bool(std::ostream&, bool);
    static void s_dvec3(std::ostream&, glm::dvec3 const &);
    static void s_vec4(std::ostream&, glm::vec4 const &);
};