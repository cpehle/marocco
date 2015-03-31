#pragma once
// neuron analog parameter transformation for the HMF System

#include <type_traits>
#include <stdexcept>
#include <numeric>
#include <vector>
#include <array>

#include "euter/typedcellparametervector.h"
#include "HMF/NeuronCollection.h"

#include "marocco/config.h"
#include "marocco/graph.h"

#include "marocco/parameter/detail.h"

namespace marocco {
namespace parameter {

struct NeuronSharedParameterRequirements
{
	typedef void return_type;
	typedef HMF::Coordinate::NeuronOnHICANN neuron_t;
	typedef HMF::Coordinate::FGBlockOnHICANN group_t;

	template <CellType N>
	using cell_t = TypedCellParameterVector<N>;

	template <CellType N>
	typename std::enable_if<!detail::has_v_reset<cell_t<N>>::value, return_type>::type
	operator() (cell_t<N> const& /*unused*/, size_t /*unused*/, neuron_t const& /*unused*/)
	{
		std::stringstream ss;
		ss << "unsupported cell type: " << getCellTypeName(N);
		throw std::runtime_error(ss.str());
	}

	template <CellType N>
	typename std::enable_if<detail::has_v_reset<cell_t<N>>::value, return_type>::type
	operator() (cell_t<N> const& v, size_t neuron_bio_id, neuron_t const& n)
	{
		auto const& cellparams = v.parameters()[neuron_bio_id];
		mVResetValues[n.toSharedFGBlockOnHICANN().id()].push_back(cellparams.v_reset);
	}

	double get_mean_v_reset(group_t const& g) const
	{
		auto const& vals = mVResetValues[g.id()];
		return std::accumulate(vals.begin(), vals.end(), 0) / vals.size();
	}

private:
	std::array<std::vector<double>, group_t::enum_type::end> mVResetValues;
};

struct TransformNeurons
{
	typedef void return_type;
	typedef size_t size_type;
	typedef chip_type<hardware_system_t>::type chip_t;
	typedef HMF::NeuronCollection calib_t;
	typedef HMF::Coordinate::NeuronOnHICANN neuron_t;

	template <CellType N>
		using cell_t = TypedCellParameterVector<N>;

	TransformNeurons(NeuronSharedParameterRequirements const& s)
		: shared_parameters(s) {}

	template <CellType N>
	return_type operator() (
		cell_t<N> const& /*unused*/,
		calib_t const& /*unused*/,
		size_t /*unused*/,
		neuron_t const& /*unused*/,
		chip_t& /*unused*/) const
	{
		std::stringstream ss;
		ss << "unsupported cell type: " << getCellTypeName(N);
		throw std::runtime_error(ss.str());
	}

	// AdEx Parameter Transformation
	return_type operator() (
		cell_t<CellType::EIF_cond_exp_isfa_ista> const& v,
		calib_t const& calib,
		size_t neuron_bio_id,
		neuron_t const& neuron_hw_id,
		chip_t& chip) const;

	// LIF Parameter Transformation
	return_type operator() (
		cell_t<CellType::IF_cond_exp> const& v,
		calib_t const& calib,
		size_t neuron_bio_id,
		neuron_t const& neuron_hw_id,
		chip_t& chip) const;

private:
	NeuronSharedParameterRequirements const& shared_parameters;
};


void transform_analog_neuron(
	HMF::NeuronCollection const& calib,
	Population const& pop,
	size_t neuron_bio_id,
	HMF::Coordinate::NeuronOnHICANN const& neuron_hw_id,
	TransformNeurons& visitor,
	chip_type<hardware_system_t>::type& chip);

} // namespace parameter
} // namespace marocco