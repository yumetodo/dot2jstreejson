#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <regex>
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include "../3rd_party/string_split/include/string_split.hpp"
std::string unescaper(const std::string& s)
{
	std::size_t escape_pos = s.find_first_of('\\');
	if (std::string::npos == escape_pos) return s;
	std::string re;
	std::size_t pre_pos = 0;
	re.reserve(s.size() - 1);
	for (;
		std::string::npos != escape_pos; 
		pre_pos = escape_pos, escape_pos = s.find_first_of('\\', escape_pos)
	) {
		const auto insert_pos = re.size();
		re.resize(insert_pos + escape_pos - pre_pos + 1);
		std::copy(s.begin() + pre_pos, s.begin() + (insert_pos + 1), re.begin() + insert_pos);
	}
	const auto insert_pos = re.size();
	re.resize(insert_pos + s.size() - pre_pos + 1);
	std::copy(s.begin() + pre_pos, s.end(), re.begin() + insert_pos);
	return re;
}
namespace detail {
	template<typename CharType>
	class count_manager {
		CharType corresponding_char_;
		std::reference_wrapper<const std::basic_string<CharType>> s_;
		std::size_t begin_char_cnt_;
		std::size_t corresponding_char_cnt_;
		std::reference_wrapper<const std::size_t> i_;
	public:
		count_manager() = delete;
		count_manager(const count_manager&) = delete;
		count_manager(count_manager&&) = delete;
		count_manager& operator=(const count_manager&) = delete;
		count_manager& operator=(count_manager&&) = delete;
		count_manager(CharType corresponding_char, const std::basic_string<CharType>& s, const std::size_t& i)
			: corresponding_char_(corresponding_char), s_(s), begin_char_cnt_(1), corresponding_char_cnt_(), i_(i)
		{}
		void operator++()
		{
			if (corresponding_char_ == s_.get()[i_.get()]) {
				++corresponding_char_cnt_;
			}
			else {
				++begin_char_cnt_;
			}
		}
		bool is_corresponding() { return begin_char_cnt_ == corresponding_char_cnt_; }
	};
}
template<typename CharType>
std::size_t find_corresponding_charactor(
	const std::basic_string<CharType>& s, const std::size_t front_pos, const std::size_t last_pos, 
	const CharType corresponding_char
)
{
	const CharType chars[] = { s[front_pos], corresponding_char, CharType(0) };
	std::size_t i = front_pos + 1;
	for (
		detail::count_manager<CharType> cnt(corresponding_char, s, i);
		!cnt.is_corresponding()
		&& (i = s.find_first_of(chars, i)) <= last_pos
		&& std::basic_string<CharType>::npos != i;
		++cnt
	);
	return i;
}
template<typename CharType>
std::size_t rfind_corresponding_charactor(
	const std::basic_string<CharType>& s, const std::size_t front_pos, const std::size_t last_pos,
	const CharType corresponding_char
)
{
	const CharType chars[] = { s[front_pos], corresponding_char, CharType(0) };
	std::size_t i = last_pos - 1;
	for (
		detail::count_manager<CharType> cnt(corresponding_char, s, i);
		!cnt.is_corresponding()
		&& front_pos <= (i = s.find_first_of(chars, i))
		&& std::basic_string<CharType>::npos != i;
		++cnt
	);
	return i;
}

struct function_information {
	using string = std::string;
	using nodeid_t = std::uint64_t;
	string return_type;
	string name_space;
	string name;
	string template_args;
	string args;
	string qualifier;
	nodeid_t id;
	function_information() = delete;
	function_information(const string& s);
	bool operator==(const function_information& o) const noexcept
	{
		return this->full_string_ == o.full_string_;
	}
	bool operator!=(const function_information& o) const noexcept { return !(*this == o); }
	bool operator<(const function_information& o) const noexcept
	{
		return this->name < o.name || this->name_space < o.name_space 
			|| this->template_args < o.template_args || this->args < o.args 
			|| this->return_type < o.return_type || this->qualifier < o.qualifier;
	}
	bool operator<=(const function_information& o) const noexcept { return *this == o || *this < o; }
	bool operator>(const function_information& o) const noexcept { return !(*this <= o); }
	bool operator>=(const function_information& o) const noexcept { return !(*this < o); }
private:
	string full_string_;
	friend std::string to_string(const function_information&);
	static bool is_pure_c_function(const string& s)
	{
		return string::npos == s.find_first_of('(');
	}
	static bool has_namespace(const string& s){
		return string::npos != s.find("::");
	}
	static std::size_t find_args_front(const string& s, std::size_t back_pos = 0)
	{
		if (0 == back_pos) back_pos = find_args_back(s);
		return rfind_corresponding_charactor(s, 0, back_pos, '(');
	}
	static std::size_t find_args_back(const string& s) { return s.find_first_of(')'); }
};
std::string to_string(const function_information& info) { return info.full_string_; }
std::ostream& operator<<(std::ostream& os, const function_information& info) { 
	os << to_string(info);
	return os;
}
function_information::function_information(const string& s)
	: return_type(), name_space(), name(), template_args(), args(), qualifier(), full_string_(s)
{
	if (s.empty()) throw std::invalid_argument("function_information::function_information");
	if (is_pure_c_function(s)) {
		this->name = s;
	}
	else {
		const auto args_back_pos = find_args_back(s);
		if(string::npos == args_back_pos) throw std::invalid_argument("function_information::function_information");
		const auto args_front_pos = find_args_front(s, args_back_pos);
		if (string::npos == args_front_pos) throw std::invalid_argument("function_information::function_information");
		auto name_like = s.substr(0, args_front_pos);
		//is template function
		if ('>' == name_like.back() && string::npos == name_like.find("operator")) {
			const auto targ_front_pos = rfind_corresponding_charactor(name_like, 0, string::npos, '<');
			if (string::npos == targ_front_pos) throw std::invalid_argument("function_information::function_information");
			this->template_args = name_like.substr(targ_front_pos + 1, name_like.size() - targ_front_pos - 2);
			name_like.erase(targ_front_pos);
		}
		if (!has_namespace(name_like) || string::npos == name_like.find_first_of('<')) {
			auto tmp = std::move(name_like) | split(' ') >> at_last();
			if (!tmp.front().empty()) this->return_type = std::move(tmp.front());
			this->name = std::move(tmp.back());
		}
		else {
			auto name_and_other = std::move(name_like) | split(':') >> at_last();
			auto& without_name = name_and_other.front();
			//戻り値の型がtemplate引数を含む可能性を考える
			if (string::npos == (without_name | split(' ') >> front()).find_first_of('<')) {
				//戻り値の型がtemplate引数を含まない
				auto returntype_and_namespace = std::move(without_name) | split(' ') >> at_first();
				if (returntype_and_namespace.back().empty()) {
					//戻り値の型なんてなかった
					this->name_space = std::move(returntype_and_namespace.front());
				}
				else {
					this->return_type = std::move(returntype_and_namespace.front());
					this->name_space = std::move(returntype_and_namespace.back());
				}
			}
			else {
				//戻り値の型がtemplate引数を含む可能性がある
				const auto targ_front = without_name.find_first_of('<');
				const auto targ_back = find_corresponding_charactor(without_name, targ_front, without_name.size() - 1, '>');
				if (string::npos == targ_back) throw std::invalid_argument("function_information::function_information");
				//template引数以降
				auto after_targ = without_name.substr(targ_back + 1);
				auto maybe_namespace = after_targ | split(' ') >> at_first().back();
				if (maybe_namespace.empty()) {
					//戻り値の型なんてなかった
					this->name_space = std::move(after_targ);
				}
				else {
					this->return_type = without_name.substr(0, targ_back + after_targ.find_first_of(' '));
					this->name_space = std::move(maybe_namespace);
				}
			}
			this->name = std::move(name_and_other.back());
		}
		this->args = s.substr(args_front_pos + 1, args_back_pos - args_front_pos - 1);
		this->qualifier = s.substr(args_back_pos + 1);
	}
}
class calltree_manager {
public:
	using nodeid_t = std::uint64_t;
	using string = std::string;
	calltree_manager() = default;
	calltree_manager(const calltree_manager&) = default;
	calltree_manager(calltree_manager&&) = default;
	calltree_manager& operator=(const calltree_manager&) = default;
	calltree_manager& operator=(calltree_manager&&) = default;
	calltree_manager(std::istream& is);
	std::vector<nodeid_t> find_by_fullname(const std::string& s) const
	{
		std::regex reg(s);
	}
private:
	using nodeid_parse_data_t = std::pair<nodeid_t, std::size_t>;
	std::vector<function_information> info_;
	std::unordered_map<nodeid_t, std::reference_wrapper<function_information>> id_info_mapper_;
	std::unordered_map<nodeid_t, std::vector<nodeid_t>> node_;
	static std::size_t find_nodeid_pos(const string& s, std::size_t off = 0u);
	nodeid_parse_data_t s2nodeid(const string& s, std::size_t first_pos);
	void register_function_info(nodeid_parse_data_t first_node_data, const string& s);
	void register_node(nodeid_parse_data_t first_node_data, const string& s);
	void parse_line(const string& s);
};
calltree_manager::calltree_manager(std::istream & is)
{
	for (string buf; std::getline(is, buf);) {
		this->parse_line(buf);
	}
}
std::size_t calltree_manager::find_nodeid_pos(const string & s, std::size_t off)
{
	const size_t node_front = s.find("Node", off);
	return (string::npos == node_front) ? string::npos : node_front + 4;
}
calltree_manager::nodeid_parse_data_t calltree_manager::s2nodeid(const string & s, std::size_t first_pos)
{
	errno = 0;
	char* endptr = nullptr;
	nodeid_t re = std::strtoull(s.c_str(), &endptr, 16);
	return{ re, (nullptr == endptr || 0 != errno) ? 0 : endptr - s.c_str() };
}
void calltree_manager::register_function_info(nodeid_parse_data_t first_node_data, const string& s)
{
	if (0 == this->node_.count(first_node_data.first)) this->node_.insert(std::make_pair(first_node_data.first, std::vector<nodeid_t>{}));
	static constexpr const char* label_begin = R"(label="{)";
	static constexpr const char* label_end = R"(}"])";
	const size_t info_front = s.find(label_begin, first_node_data.second) + std::char_traits<char>::length(label_begin);
	this->info_.emplace_back(unescaper(s.substr(info_front, s.find(label_end, info_front))));
	this->id_info_mapper_[first_node_data.first] = this->info_.back();
	this->info_.back().id = first_node_data.first;
}

void calltree_manager::register_node(nodeid_parse_data_t first_node_data, const string & s)
{
	const auto child_node_data = s2nodeid(s, find_nodeid_pos(s, first_node_data.second));
	if (0 == child_node_data.second) throw std::invalid_argument("calltree_manager::register_node");
	auto& child_list = this->node_[first_node_data.first];
	if (child_list.empty()) child_list.reserve(8);
	child_list.push_back(child_node_data.first);
}

void calltree_manager::parse_line(const string & s)
{
	const auto nodeid_pos = find_nodeid_pos(s);
	if (string::npos == nodeid_pos) return;
	const auto first_node_data = s2nodeid(s, nodeid_pos);

	if (string::npos == s.find("label", first_node_data.second)) {
		this->register_function_info(first_node_data, s);
	}
	else {
		this->register_node(first_node_data, s);
	}
}

int main(){}
