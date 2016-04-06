#pragma once

#include "hal/Coordinate/L1.h"
#ifndef PYPLUSPLUS
#include "hal/Coordinate/typed_array.h"
#endif // !PYPLUSPLUS

#include "pywrap/compat/array.hpp"
#include <boost/serialization/nvp.hpp>

namespace pymarocco {

class Routing
{
public:
	typedef HMF::Coordinate::HLineOnHICANN HLineOnHICANN;
	typedef HMF::Coordinate::VLineOnHICANN VLineOnHICANN;

#ifndef PYPLUSPLUS
	template <typename V, typename K>
	using typed_array = HMF::Coordinate::typed_array<V, K>;
	typedef typed_array<typed_array<bool, HLineOnHICANN>, VLineOnHICANN> switches_t;
#endif // !PYPLUSPLUS

	Routing();

	/// toggle shuffling of crossbar switches
	/// i.e. which crossbar switches are considered
	/// first when e.g. going from horizontal L1 lanes to vertical
	///
	/// TODO: improve description
	///
	/// default: true
	bool shuffle_crossbar_switches;

	/// whether the routing configuration has been changed or not. This is /
	//important because actual hardware configuration can no longer been /
	//carried out with e.g. different crossbar layout. Halbe doesn't like to get
	//non-existant switches set.
	bool is_default() const;

#ifndef PYPLUSPLUS
	switches_t crossbar;
#endif // !PYPLUSPLUS

	// FIXME: add some crossbar interfaces, because python wrapping is not
	// really nice nor functional for arrays of arrays.
	bool cb_get(VLineOnHICANN const& x, HLineOnHICANN const& y) const;
	void cb_set(VLineOnHICANN const& x, HLineOnHICANN const& y, bool b);
	void cb_clear();
	void cb_reset();

	size_t horizontal_line_swap;
	size_t vertical_line_swap;
	size_t syndriver_chain_length;

	double max_distance_factor;

	// parameters for global DijkstraRouting
	size_t weight_Vertical;
	size_t weight_Horizontal;
	size_t weight_SPL1;
	size_t weight_StraightHorizontal;
	size_t weight_StraightVertical;
	size_t weight_CongestionFactor;

	bool _is_default;

private:
#ifndef PYPLUSPLUS
	friend class boost::serialization::access;
	template<typename Archive>
	void serialize(Archive& ar, unsigned int const)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("crossbar", crossbar)
		   & make_nvp("hline_swap", horizontal_line_swap)
		   & make_nvp("vline_swap", vertical_line_swap)
		   & make_nvp("chain_length", syndriver_chain_length)
		   & make_nvp("distance_factor", max_distance_factor)
		   & make_nvp("_is_default", _is_default)

		   & make_nvp("w_vert", weight_Vertical)
		   & make_nvp("w_hor", weight_Horizontal)
		   & make_nvp("w_spl1", weight_SPL1)
		   & make_nvp("w_hstraight", weight_StraightHorizontal)
		   & make_nvp("w_vstraiggt", weight_StraightVertical)
		   & make_nvp("w_congest", weight_CongestionFactor)
		   & make_nvp("shuffle_crossbar_switches", shuffle_crossbar_switches);
	}
#endif // !PYPLUSPLUS
};

} // pymarocco
