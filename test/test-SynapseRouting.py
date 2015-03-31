import copy, unittest, random
import numpy as np
from ester import Ester
from pymarocco import *
from pyhalbe.Coordinate import *
from pyhmf import *

"""
This test is supposed to test the number of SynapseDrivers allocated for a
variable number of source populations. The more sources there are the more
SynapseDrivers are needed, and the bigger a neuron is, the less SynapseDrivers
are needed.

The expected number of needed SynapseDrivers is given by the number of source
neurons divided by half the neuron size (half the DenMems is only reachable via
the other synapse array) and divided by two (two sources can connect via a
single SynapseDriver).

CAVEAT0: currently, the MergerRouting can split the N<63 source neurons into two
routes. You then need to sum op the total number.

CAVEAT1: there's currently no way of autonomic assertion. You need to reed the
requirements from the log output.
"""
class TestSynapseRouting(unittest.TestCase):

    def setUp(self, backend=PyMarocco.None):
        self.marocco = PyMarocco()
        self.marocco.backend = backend

    def tearDown(self):
        del self.marocco

    def chip(self, hicann):
        return HICANNGlobal(Enum(hicann))

    def test_TwoNeuron(self):
        with Ester() as ester:
            setup(marocco=self.marocco)

            # create neuron with v_rest below v_thresh
            target = Population(1, EIF_cond_exp_isfa_ista)

            N = 16            # number of source populations
            NEURON_SIZE = 4   # default neuron size

            self.marocco.placement.setDefaultNeuronSize(NEURON_SIZE)

            p = [ Population(1, EIF_cond_exp_isfa_ista) for i in range(N) ]

            # place target on HICANN 0
            target_chip = self.chip(0)
            self.marocco.placement.add(target, target_chip)

            connector = AllToAllConnector(
                    allow_self_connections=True,
                    weights=1.)


            for pop in p:
                proj = Projection(pop, target, connector, target='excitatory')

                # place source neuron
                target_chip = self.chip(1)
                self.marocco.placement.add(pop, target_chip)

            # start simulation
            run(10) # in ms
            end()

            # expected number of needed SynapseDrivers is given by the number
            # of source neurons divided by half the neuron size (half the
            # denmem is only reachable via the other synapse array) and divided
            # by two (two sources can connect via a single SynapseDriver).
            EXPECTED_NUM_SYNAPSEDRIVER = N/(NEURON_SIZE/2)/2
            print 'EXPECTED %d' % EXPECTED_NUM_SYNAPSEDRIVER

            # make sure we have no synapse loss
            self.assertEqual(0, self.marocco.stats.getSynapseLoss())


if __name__ == '__main__':
    unittest.main()