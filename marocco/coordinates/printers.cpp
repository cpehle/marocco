#include "marocco/coordinates/printers.h"

namespace marocco {
namespace detail {

std::ostream& operator<<(std::ostream& os, pretty_printer<L1Route> pr)
{
	os << "(L1Route";
	std::string space(pr.indent + 9, ' ');
	std::string sep = "\n" + space;
	auto const& segments = pr.what.segments();
	if (segments.begin() != segments.end()) {
		os << " ";
		std::copy(
			segments.begin(), std::prev(segments.end()),
			std::ostream_iterator<L1Route::segment_type>(os, sep.c_str()));
		os << segments.back();
	}
	os << ")";
	return os;
}

std::ostream& operator<<(std::ostream& os, pretty_printer<L1RouteTree> pr)
{
	std::string space(pr.indent + 2, ' ');
	os << "(L1RouteTree\n" << space;
	os << pretty_printed(pr.what.head(), pr.indent + 2);
	if (pr.what.has_tails()) {
		os << "\n" << space << "(tails";
		for (L1RouteTree const& tail : pr.what.tails()) {
			os << "\n  " << space << pretty_printed(tail, pr.indent + 4);
		}
		os << ")";
	}
	os << ")";
	return os;
}

std::ostream& operator<<(std::ostream& os, pretty_printer<LogicalNeuron> pr)
{
	os << "LogicalNeuron";
	if (pr.what.is_external()) {
		os << "::external(" << pr.what.m_external_identifier
		   << ", " << pr.what.m_external_index << ")";
	} else {
		os << "::on(" << pr.what.m_block << ")";
		std::string space(pr.indent + 2, ' ');
		for (auto const& chunk : pr.what.m_chunks) {
			os << "\n" << space << ".add(" << chunk.first << ", " << chunk.second << ")";
		}
		os << "\n" << space << ".done()";
	}
	return os;
}

} // namespace detail
} // namespace marocco
