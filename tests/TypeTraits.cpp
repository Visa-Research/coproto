
#include "coproto/Common/TypeTraits.h"
#include "coproto/Common/Defines.h"
#include "coproto/Common/span.h"
#include <vector>
#include <array>
#include <string>
#include <type_traits>

namespace coproto
{
	namespace tests {


		//using T = std::string;

		//	// must have value_type
		//typename T::value_type;

		//	// must have a data() member fn
		//decltype(std::declval<T>().data());

		//	// must return value_type*
		//static_assert(std::is_convertible<
		//	decltype(std::declval<T>().data()),
		//	typename T::value_type*
		//>::value, "");

		static_assert(has_data_member_func<std::array<char, 5>>::value, "");
		static_assert(has_data_member_func<std::array<long long, 5>>::value, "");
		static_assert(has_data_member_func<std::vector<char>>::value, "");
		static_assert(has_data_member_func<std::vector<long long>>::value, "");
		static_assert(has_data_member_func<span<char>>::value, "");
		static_assert(has_data_member_func<span<long long>>::value, "");
		static_assert(has_data_member_func<std::string>::value, "");
		static_assert(has_data_member_func<int>::value == false, "");
		//static_assert(has_resize_member_func_v<int>::value == false, "");

		static_assert(has_size_member_func<std::array<char, 5>>::value, "");
		static_assert(has_size_member_func<std::array<long long, 5>>::value, "");
		static_assert(has_size_member_func<std::vector<char>>::value, "");
		static_assert(has_size_member_func<std::vector<long long>>::value, "");
		static_assert(has_size_member_func<span<char>>::value, "");
		static_assert(has_size_member_func<span<long long>>::value, "");
		static_assert(has_size_member_func<std::string>::value, "");
		static_assert(has_size_member_func<int>::value == false, "");


		static_assert(has_size_type<std::array<char, 5>>::value, "");
		static_assert(has_size_type<std::array<long long, 5>>::value, "");
		static_assert(has_size_type<std::vector<char>>::value, "");
		static_assert(has_size_type<std::vector<long long>>::value, "");
		static_assert(has_size_type<span<char>>::value, "");
		static_assert(has_size_type<span<long long>>::value, "");
		static_assert(has_size_type<std::string>::value, "");
		static_assert(has_size_type<int>::value == false, "");
		
		static_assert(has_size_member_func<std::array<char, 5>>::value, "");
		static_assert(has_size_member_func<std::array<long long, 5>>::value, "");
		static_assert(has_size_member_func<std::vector<char>>::value, "");
		static_assert(has_size_member_func<std::vector<long long>>::value, "");
		static_assert(has_size_member_func<span<char>>::value, "");
		static_assert(has_size_member_func<span<long long>>::value, "");
		static_assert(has_size_member_func<std::string>::value, "");
		static_assert(has_size_member_func<int>::value == false, "");

		static_assert(has_resize_member_func<std::array<char, 5>>::value == false, "");
		static_assert(has_resize_member_func<std::array<long long, 5>>::value == false, "");
		static_assert(has_resize_member_func<span<char>>::value == false, "");
		static_assert(has_resize_member_func<span<long long>>::value == false, "");
		static_assert(has_resize_member_func<std::vector<char>>::value, "");
		static_assert(has_resize_member_func<std::vector<long long>>::value, "");
		static_assert(has_resize_member_func<std::string>::value, "");
		static_assert(has_resize_member_func<int>::value == false, "");

		static_assert(is_trivial_container<std::string>::value == true, "");
		static_assert(is_trivial_container<std::array<char, 5>>::value == false, "");
		static_assert(is_trivial_container<std::array<long long, 5>>::value == false, "");
		static_assert(is_trivial_container<span<char>>::value, "");
		static_assert(is_trivial_container<span<long long>>::value, "");
		static_assert(is_trivial_container<std::vector<char>>::value, "");
		static_assert(is_trivial_container<std::vector<long long>>::value, "");
		static_assert(is_trivial_container<std::string>::value, "");

		static_assert(is_trivial_container<span<span<char>>>::value == false, "");
		static_assert(is_trivial_container<span<span<long long>>>::value == false, "");
		static_assert(is_trivial_container<std::vector<std::vector<char>>>::value == false, "");
		static_assert(is_trivial_container<std::vector<std::vector<long long>>>::value == false, "");
		static_assert(is_trivial_container<int>::value == false, "");

		static_assert(is_resizable_trivial_container<std::string>::value == true, "");
		static_assert(is_resizable_trivial_container<std::array<char, 5>>::value == false, "");
		static_assert(is_resizable_trivial_container<std::array<long long, 5>>::value == false, "");
		static_assert(is_resizable_trivial_container<span<char>>::value == false, "");
		static_assert(is_resizable_trivial_container<span<long long>>::value == false, "");
		static_assert(is_resizable_trivial_container<std::vector<char>>::value, "");
		static_assert(is_resizable_trivial_container<std::vector<long long>>::value, "");
		static_assert(is_resizable_trivial_container<std::string>::value, "");
		static_assert(is_resizable_trivial_container<int>::value == false, "");



		struct Base {};


		struct Derived_int : public Base
		{
			Derived_int(int)
			{}
		};

		struct Derived_int_str : public Base
		{
			Derived_int_str(int, std::string)
			{}
		};

		static_assert(is_poly_emplaceable<Base, Base>::value == true, "");
		static_assert(is_poly_emplaceable<Base, Derived_int, int>::value == true, "");
		static_assert(is_poly_emplaceable<Base, Derived_int_str, int, std::string>::value == true, "");
		static_assert(is_poly_emplaceable<Base, std::string>::value == false, "");
		static_assert(is_poly_emplaceable<Base, std::string, int>::value == false, "");
		static_assert(is_poly_emplaceable<Base, std::string, int, std::string>::value == false, "");

	}

}
