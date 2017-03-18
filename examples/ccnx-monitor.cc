/*
 * Copyright (c) 2016, Xerox Corporation (Xerox) and Palo Alto Research Center, Inc (PARC)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL XEROX OR PARC BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* ################################################################################
 * #
 * # PATENT NOTICE
 * #
 * # This software is distributed under the BSD 2-clause License (see LICENSE
 * # file).  This BSD License does not make any patent claims and as such, does
 * # not act as a patent grant.  The purpose of this section is for each contributor
 * # to define their intentions with respect to intellectual property.
 * #
 * # Each contributor to this source code is encouraged to state their patent
 * # claims and licensing mechanisms for any contributions made. At the end of
 * # this section contributors may each make their own statements.  Contributor's
 * # claims and grants only apply to the pieces (source code, programs, text,
 * # media, etc) that they have contributed directly to this software.
 * #
 * # There is no guarantee that this section is complete, up to date or accurate. It
 * # is up to the contributors to maintain their portion of this section and up to
 * # the user of the software to verify any claims herein.
 * #
 * # Do not remove this header notification.  The contents of this section must be
 * # present in all distributions of the software.  You may only modify your own
 * # intellectual property statements.  Please provide contact information.
 *
 * - Palo Alto Research Center, Inc
 * This software distribution does not grant any rights to patents owned by Palo
 * Alto Research Center, Inc (PARC). Rights to these patents are available via
 * various mechanisms. As of January 2016 PARC has committed to FRAND licensing any
 * intellectual property used by its contributions to this software. You may
 * contact PARC at cipo@parc.com for more information or visit http://www.ccnx.org
 */


// Network topology
//
//  n0
//     \ 5 Mb/s, 2ms
//      \          1.5Mb/s, 10ms
//       n2 -------------------------n3
//      /
//     / 5 Mb/s, 2ms
//   n1
//

// This is primarily to expose the simplicity of loadbalancing across nodes.
// Nodes n0 and n1 are hosting nProducers
// Node n3 hosts consumers (number = nProducers) which will express random interests.
// Node n2 will route the requests to both n0 and n1. n0 and n1 will respond but the PIT table on n2 should only forward 1/2 of these responses.


#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ccns3Sim-module.h"

using namespace ns3;
using namespace ns3::ccnx;

NS_LOG_COMPONENT_DEFINE ("ccnx-monitor");

static void
PrintVector(std::vector<double> v)
{
    for (int i = 0; i < v.size() - 1; i++) {
        std::cerr << v.at(i) << ",";
    }
    std::cerr << v.at(v.size() - 1) << std::endl;
}

static void
RunSimulation (uint32_t nPrefixes, uint32_t totalTime, uint32_t numberContents, uint32_t cacheSize)
{
  NS_LOG_INFO ("Number of Prefixes served are = " << nPrefixes );
  Time::SetResolution (Time::NS);
  NodeContainer nodes;
  nodes.Create (4);

  NodeContainer n0n2 = NodeContainer (nodes.Get (0), nodes.Get (2));
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer n3n2 = NodeContainer (nodes.Get (3), nodes.Get (2));

  // We create the channels first without any IP addressing information
  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d0d2 = p2p.Install (n0n2);
  NetDeviceContainer d1d2 = p2p.Install (n1n2);

  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (1500000)));
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer d3d2 = p2p.Install (n3n2);

  /*
   * Setup a Forwarder and associate it with the nodes created.
   */
  CCNxStackHelper ccnxStack;
  CCNxStandardForwarderHelper standardHelper;
  ccnxStack.SetForwardingHelper (standardHelper);

  // Set the contnet store capacity
  CCNxStandardContentStoreFactory contentStoreFactory;
  contentStoreFactory.Set("ObjectCapacity", IntegerValue(cacheSize));
  standardHelper.SetContentStoreFactory(contentStoreFactory);

  NfpRoutingHelper nfpHelper;
  nfpHelper.Set ("HelloInterval", TimeValue (Seconds (5)));
#if DEBUG
  Ptr<OutputStreamWrapper> trace = Create<OutputStreamWrapper> (&std::cout);
  nfpHelper.PrintNeighborTableAllEvery (Time (Seconds (5)), trace);
  nfpHelper.PrintRoutingTableAllEvery (Time (Seconds (5)), trace);
#endif
  ccnxStack.SetRoutingHelper (nfpHelper);
  ccnxStack.Install (nodes.Get (2));
  ccnxStack.Install (nodes.Get (3));
  ccnxStack.AddInterfaces (d3d2);

  // Do NOT install a cache on the end nodes
  CCNxStackHelper nodeStackHelper;
  nodeStackHelper.SetRoutingHelper (nfpHelper);
  nodeStackHelper.Install (nodes.Get (0));
  nodeStackHelper.Install (nodes.Get (1));
  ccnxStack.AddInterfaces (d0d2);
  ccnxStack.AddInterfaces (d1d2);
  // nodeStackHelper.Install (nodes.Get (0));
  // nodeStackHelper.Install (nodes.Get (1));

  const std::string pre = "ccnx:/name=simple/name=producer";
  uint32_t size = 256; // size (in bytes) of each content -- not important here
  uint32_t count = numberContents;
  Ptr <CCNxContentRepository> repo = Create <CCNxContentRepository> (Create <CCNxName> (pre), size, count);
  CCNxProducerHelper producerHelper (repo);
  ApplicationContainer pn0 = producerHelper.Install (nodes.Get (3));
  pn0.Start (Seconds (0.0));
  pn0.Stop (Seconds (totalTime + 1));

  CCNxConsumerHelper consumerHelper (repo);
  consumerHelper.SetAttribute ("RequestInterval", TimeValue (Seconds (1))); // request once per second
  ApplicationContainer cn3 = consumerHelper.Install (nodes.Get (0));
  cn3.Start (Seconds (1.0));
  cn3.Stop (Seconds (totalTime));

  CCNxMonitorHelper monitorHelper (repo);
  monitorHelper.SetAttribute ("RequestInterval", TimeValue (MilliSeconds (50)));
  ApplicationContainer monitorContainer = monitorHelper.Install (nodes.Get (1));
  monitorContainer.Start (Seconds (1.0));
  monitorContainer.Stop (Seconds (totalTime));

  Simulator::Stop (Seconds (totalTime - 1));

  // Run the simulator and execute all the events
  Simulator::Run ();
  Simulator::Destroy ();

  int total = 100;
  Ptr<CCNxMonitor> monitor = DynamicCast<CCNxMonitor>(monitorContainer.Get(0));
  std::vector<double> observedHistogram = monitor->GetObservedHistogram();
  std::vector<double> actualHistogram = repo->GetPopularityHistogram(total);
  std::vector<double> repoSampledHistogram = repo->GetSampledHistogram();

  PrintVector(observedHistogram);
  PrintVector(actualHistogram);
  PrintVector(repoSampledHistogram);
}

int
main (int argc, char *argv[])
{
  uint32_t nPrefixes = 2;
  uint32_t totalTime = 500;
  uint32_t numberContents = 10;
  uint32_t cacheSize = numberContents - 1;
  CommandLine cmd;
  cmd.AddValue ("nPrefixes", "Number of Prefixes to simulate", nPrefixes);
  cmd.AddValue ("time", "Total simulation time", totalTime);
  cmd.AddValue ("numberContents", "Number of contents", numberContents);
  cmd.AddValue ("cacheSize", "Number of contents", cacheSize);
  cmd.Parse (argc, argv);

  RunSimulation (nPrefixes, totalTime, numberContents, cacheSize);
  return 0;
}
