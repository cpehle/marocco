#!/usr/bin/env python
# -*- coding: utf8 -*-

import unittest
import pyhmf as pynn
import pymarocco
from pyhalbe.Coordinate import *
import debug_config

class STPTest(unittest.TestCase):
    def test_basic(self):
        """
        tests whether synapses with short term plasticity are routed correctly.

        Build a minimal network with 1 neuron and 3 spike sources each
        connecting to with a different STP setting (depression, facilitation,
        static) to the neuron.
        Then check that the 3 synapses are routed via 3 different synaspe
        drivers and that STP mode of the synapse drivers is as expected.
        """
        marocco=pymarocco.PyMarocco()
        marocco.backend = pymarocco.PyMarocco.None
        marocco.placement.setDefaultNeuronSize(4)
        marocco.wafer_cfg = "wafer.xml"
        used_hicann = HICANNGlobal(Enum(0))

        pynn.setup(marocco=marocco)

        p1 = pynn.Population(1, pynn.IF_cond_exp)

        # place to a certain HICANN to be able to extract config data afterwards
        marocco.placement.add(p1,used_hicann)

        s1 = pynn.Population(1, pynn.SpikeSourcePoisson, {'rate':5.})
        s2 = pynn.Population(1, pynn.SpikeSourcePoisson, {'rate':5.})
        s3 = pynn.Population(1, pynn.SpikeSourcePoisson, {'rate':5.})

        depression = pynn.SynapseDynamics(fast=pynn.TsodyksMarkramMechanism(U=0.4,tau_rec=200., tau_facil=0.))
        facilitation = pynn.SynapseDynamics(fast=pynn.TsodyksMarkramMechanism(U=0.4,tau_rec=0., tau_facil=200.))
        static = None
        prj1 = pynn.Projection(s1,p1,pynn.OneToOneConnector(weights=0.05), synapse_dynamics=depression, target="excitatory")
        prj2 = pynn.Projection(s2,p1,pynn.OneToOneConnector(weights=0.05), synapse_dynamics=facilitation, target="excitatory")
        prj3 = pynn.Projection(s3,p1,pynn.OneToOneConnector(weights=0.05), synapse_dynamics=static, target="excitatory")

        p1.record()

        pynn.run(1.)

        h = debug_config.load_hicann_cfg(marocco.wafer_cfg, used_hicann)

        # There should be 3 active drivers with 3 different STP modes
        drivers = {}
        num_active_drivers = 0
        for driver in iter_all(SynapseDriverOnHICANN):
            drv_cfg = h.synapses[driver]
            if drv_cfg.is_enabled():
                num_active_drivers += 1
                if drv_cfg.is_stf():
                    drivers['facilitation'] = driver
                elif drv_cfg.is_std():
                    drivers['depression'] = driver
                else:
                    drivers['static'] = driver

        self.assertEqual(num_active_drivers, 3)
        self.assertEqual(len(drivers), 3)

        # check that synapses are on the drivers with the correct mode
        for src,mode in [(s1,'depression'), (s2,'facilitation'), (s3,'static')]:
            addr = marocco.getStats().getHwId(debug_config.get_bio_id(src,0))[0].addr
            syns = debug_config.find_synapses(h.synapses,drivers[mode],addr)
            self.assertEqual(len(syns), 1) # the addr of the source should be found once

if __name__ == '__main__':
    unittest.main()
