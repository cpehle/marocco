#pragma once

#include "hal/Coordinate/L1.h"
#include "hal/Coordinate/typed_array.h"

#include "marocco/placement/internal/L1AddressPool.h"

namespace marocco {
namespace placement {
namespace internal {

/**
 * @brief Keeps track of available L1 addresses and modes (input/output/unused)
 *        for the DNC mergers of a single HICANN.
 */
class L1AddressAssignment
{
public:
	typedef HMF::Coordinate::DNCMergerOnHICANN index_type;

	L1AddressAssignment();

	enum class Mode {
		input,
		output,
		unused
	};

	L1AddressPool const& available_addresses(index_type const& merger) const;

	L1AddressPool& available_addresses(index_type const& merger);

	void set_mode(index_type const& merger, Mode const value);

	Mode mode(index_type const& merger) const;

	/**
	 * @brief Checks whether any DNC merger has been set to output.
	 */
	bool has_output() const;

private:
	/**
	 * @brief Mode of DNC mergers.
	 * \c Mode::unused is the default value.
	 */
	HMF::Coordinate::typed_array<Mode, index_type> m_mode;

	HMF::Coordinate::typed_array<L1AddressPool, index_type> m_address_pools;
}; // L1AddressAssignment

} // namespace internal
} // namespace placement
} // namespace marocco
