#pragma once

/**
 * Reverse Mapping
 *
 * For the interpretation of hardware results we need the mapping in reverse
 * order, meaning from hardware value to pynn value.
 * Therefore, we need to generate this mapping at the time of forward mapping.
 * And propagate it to the reverse transformation instance.
 *
 **/

#include <unordered_map>
#include <vector>
#include <iosfwd>
#include <set>

#include "marocco/config.h"
#include "marocco/graph.h"
#include "marocco/resource/HICANNManager.h"

#include "pywrap/compat/hash.hpp"

namespace marocco {
namespace placement {

class Result; // fwd dcl


/**
 * @brief representation of global L1 Address
 **/
struct hw_id
{
	HMF::Coordinate::HICANNGlobal hicann;
	HMF::Coordinate::OutputBufferOnHICANN outb;
	HMF::HICANN::L1Address addr;

	bool operator== (hw_id const& k) const;
	bool operator!= (hw_id const& k) const;
	std::ostream& operator<< (std::ostream& os) const;

	template < typename Archive >
	void serialize(Archive& ar, unsigned int const /*version*/)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("hicann", hicann)
		   & make_nvp("outb", outb)
		   & make_nvp("addr", addr);
	}
};

/**
 * @brief representation of global pynn neuron
 **/
struct bio_id
{
	size_t pop;    //<! population id
	size_t neuron; //<! relative neuron address

	bool operator== (bio_id const& v) const;
	bool operator!= (bio_id const& v) const;
	std::ostream& operator<< (std::ostream& os) const;

	template < typename Archive >
	void serialize(Archive& ar, unsigned int const /*version*/)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("pop", pop)
		   & make_nvp("neuron", neuron);
	}
};

} // namespace placement
} // namespace marocco

namespace std {
template<>
struct hash<marocco::placement::hw_id>
{
	typedef marocco::placement::hw_id type;
	size_t operator()(type const & t) const;
};

template<>
struct hash<marocco::placement::bio_id>
{
	typedef marocco::placement::bio_id type;
	size_t operator()(type const & t) const;
};
} // std


namespace marocco {
namespace placement {

/**
 * @class LookupTable
 *
 * @brief contains the actual reverse mapping.
 **/
class LookupTable
{
public:

	typedef std::unordered_map< hw_id, bio_id > hw_to_bio_map_type;
	typedef std::unordered_map< bio_id, std::vector< hw_id > > bio_to_hw_map_type;
	typedef std::unordered_map< bio_id, std::vector< HMF::Coordinate::NeuronGlobal > >
	    bio_to_denmem_map_type;

	LookupTable() = default;
	LookupTable(LookupTable const&) = default;
	LookupTable(LookupTable&&) = default;
	LookupTable(Result const &result, resource_manager_t const &mgr, graph_t const &graph);

	// hw to bio transformation
	bio_id&       operator[] (hw_id const& key);
	bio_id&       at(hw_id const& key);
	bio_id const& at(hw_id const& key) const;

	// bio to hw transformation
	std::vector< hw_id > &operator[](bio_id const &key);
	std::vector< hw_id > &at(bio_id const &key);
	std::vector< hw_id > const &at(bio_id const &key) const;

	const hw_to_bio_map_type& getHwToBioMap() const;
	hw_to_bio_map_type&       getHwToBioMap();

	const bio_to_hw_map_type& getBioToHwMap() const;
	bio_to_hw_map_type&       getBioToHwMap();

	const bio_to_denmem_map_type& getBioToDenmemMap() const;
	bio_to_denmem_map_type&       getBioToDenmemMap();

	size_t size() const;
	bool empty() const;

private:

	hw_to_bio_map_type mHw2BioMap;
	bio_to_hw_map_type mBio2HwMap;
	bio_to_denmem_map_type mBio2DenmemMap;

	friend class boost::serialization::access;
	template < typename Archive >
	void serialize(Archive &ar, unsigned int const /*version*/)
	{
		using boost::serialization::make_nvp;
		// ECM: boost::serialization does not yet support serialization of
		// std::unordered_map; to workaround this problem, we de-serialize
		// from/to a vector of key-value pairs.
		std::vector< std::pair< hw_to_bio_map_type::key_type, hw_to_bio_map_type::mapped_type > >
		    mHw2BioMapAsVector;
		std::vector< std::pair< bio_to_hw_map_type::key_type, bio_to_hw_map_type::mapped_type > >
		    mBio2HwMapAsVector;
		std::vector< std::pair< bio_to_denmem_map_type::key_type,
		                        bio_to_denmem_map_type::mapped_type > > mBio2DenmemMapAsVector;
		if (Archive::is_saving::value) {
			std::copy(mHw2BioMap.begin(), mHw2BioMap.end(), std::back_inserter(mHw2BioMapAsVector));
			std::copy(mBio2HwMap.begin(), mBio2HwMap.end(), std::back_inserter(mBio2HwMapAsVector));
			std::copy(mBio2DenmemMap.begin(), mBio2DenmemMap.end(),
			          std::back_inserter(mBio2DenmemMapAsVector));
		}
		ar & make_nvp("mHw2BioMapAsVector", mHw2BioMapAsVector);
		ar & make_nvp("mBio2HwMapAsVector", mBio2HwMapAsVector);
		ar & make_nvp("mBio2DenmemMapAsVector", mBio2DenmemMapAsVector);
		if (Archive::is_loading::value) {
			std::copy(mHw2BioMapAsVector.begin(), mHw2BioMapAsVector.end(),
			          std::inserter(mHw2BioMap, mHw2BioMap.end()));
			std::copy(mBio2HwMapAsVector.begin(), mBio2HwMapAsVector.end(),
			          std::inserter(mBio2HwMap, mBio2HwMap.end()));
			std::copy(mBio2DenmemMapAsVector.begin(), mBio2DenmemMapAsVector.end(),
			          std::inserter(mBio2DenmemMap, mBio2DenmemMap.end()));
		}
	}
};

} // namespace placement
} // namespace marocco
