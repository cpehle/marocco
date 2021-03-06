#include "marocco/placement/internal/L1AddressAssignment.h"

namespace marocco {
namespace placement {
namespace internal {

L1AddressAssignment::L1AddressAssignment()
{
	std::fill(m_mode.begin(), m_mode.end(), Mode::unused);
}

L1AddressPool& L1AddressAssignment::available_addresses(index_type const& merger)
{
	return m_address_pools[merger];
}

L1AddressPool const& L1AddressAssignment::available_addresses(index_type const& merger) const
{
	return m_address_pools[merger];
}

void L1AddressAssignment::set_mode(index_type const& merger, Mode const value)
{
	m_mode[merger] = value;
}

auto L1AddressAssignment::mode(index_type const& merger) const -> Mode
{
	return m_mode[merger];
}

bool L1AddressAssignment::has_output() const {
	return std::any_of(
	    m_mode.begin(), m_mode.end(), [](Mode value) { return value == Mode::output; });
}

} // namespace internal
} // namespace placement
} // namespace marocco
