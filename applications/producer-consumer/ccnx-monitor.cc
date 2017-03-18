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

#include <iostream>
#include <iomanip>
#include "ns3/log.h"
#include "ns3/ccnx-monitor.h"

using namespace ns3;
using namespace ns3::ccnx;
NS_LOG_COMPONENT_DEFINE ("CCNxMonitor");
NS_OBJECT_ENSURE_REGISTERED (CCNxMonitor);

static bool printConsStatsHeader = 1;
TypeId
CCNxMonitor::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ccnx::CCNxApplication::CCNxMonitor")
    .SetParent<CCNxApplication> ()
    .SetGroupName ("CCNx")
    .AddConstructor < CCNxMonitor > ()
    .AddAttribute ("RequestInterval",
                   "delay between successive Interests",
                   TimeValue (MilliSeconds (0)),
                   MakeTimeAccessor (&CCNxMonitor::m_requestInterval),
                   MakeTimeChecker ());
  return tid;
}

CCNxMonitor::CCNxMonitor ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_consumerPortal = Ptr<CCNxPortal> (0);
  m_goodInterestsSent = 0;
  m_goodContentReceived = 0;
  m_interestProcessFails = 0;
  m_contentProcessFails = 0;
  m_count = 0;
  m_sum = 0;
  m_sumSquare = 0;
}

CCNxMonitor::~CCNxMonitor ()
{
  // empty
}


void
CCNxMonitor::StartApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_consumerPortal = CCNxPortal::CreatePortal (
      GetNode (), TypeId::LookupByName ("ns3::ccnx::CCNxMessagePortalFactory"));
  m_consumerPortal->SetRecvCallback (
    MakeCallback (&CCNxMonitor::ReceiveCallback, this));

  m_requestIntervalTimer = Timer (Timer::REMOVE_ON_DESTROY);
  // m_requestIntervalTimer.SetFunction (&CCNxMonitor::GenerateTraffic, this);
  // m_requestIntervalTimer.SetDelay (m_requestInterval);
  // m_requestIntervalTimer.ScheduleOnce ();
  Simulator::Schedule(Seconds(1), &CCNxMonitor::GenerateTraffic, this);
}

void
CCNxMonitor::StopApplication (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_requestIntervalTimer.IsRunning ())
    {
      m_requestIntervalTimer.Cancel ();
    }
  m_consumerPortal->Close ();
  if (m_outstandingRequests.size ())
    {
      NS_LOG_ERROR (
        "All the outstanding requests not honored by forwarder " << m_outstandingRequests.size () << " more left behind");
    }
  else
    {
      NS_LOG_INFO (
        "All interest expressed by consumer have been honored by the network");
    }
  CCNxMonitor::ShowStatistics ();
}

void
CCNxMonitor::SetContentRepository (
  Ptr<CCNxContentRepository> repositoryPtr)
{
  NS_LOG_FUNCTION (this << repositoryPtr);
  m_globalContentRepositoryPrefix = repositoryPtr;

  // Create the packet probe container
  for (int i = 0; i < m_globalContentRepositoryPrefix->GetContentObjectCount(); i++) {
      Ptr<PacketProbe> probe = Create<PacketProbe>(i);
      probe->m_hitWaiting = false;
      probe->m_missWaiting = false;
      probe->m_count = 0;
      probe->m_limit = 100;

      for (int i = 0; i < probe->m_limit; i++) {
          probe->m_hits.push_back(false);
          probe->m_misses.push_back(false);
          probe->m_hitName.push_back(NULL);
          probe->m_missName.push_back(NULL);
      }

      m_probes.push_back(probe);
  }
}

void
CCNxMonitor::InsertOutStandingInterest (Ptr<const CCNxName> interest)
{
  NS_LOG_FUNCTION (this << interest);
  m_outstandingRequests[interest].txTime = (uint64_t) Simulator::Now ().GetMilliSeconds ();
}

void
CCNxMonitor::RemoveOutStandingInterest (Ptr<const CCNxName> interest)
{
  NS_LOG_FUNCTION (this << interest);
  uint64_t packet_latency = (uint64_t) Simulator::Now ().GetMilliSeconds () - m_outstandingRequests[interest].txTime ;
  m_sum += packet_latency;
  m_sumSquare += packet_latency * packet_latency;
  m_outstandingRequests.erase (interest);
}

bool
CCNxMonitor::FindOutStandingInterest (Ptr<const CCNxName> interest)
{
  NS_LOG_FUNCTION (this << interest);
  return m_outstandingRequests.find (interest) != m_outstandingRequests.end ();
}

void
CCNxMonitor::ReceiveCallback (Ptr<CCNxPortal> portal)
{
  NS_LOG_FUNCTION (this << portal);
  Ptr<CCNxPacket> packet;

  Time receiveTime = Simulator::Now ();

  while ((packet = portal->Recv ()))
    {
      NS_LOG_DEBUG (
        "CCNxMonitor:Received content response " << *packet << " packet dump");
      m_goodContentReceived++;
      if (packet->GetMessage ()->GetMessageType () == CCNxMessage::ContentObject)
        {
          Ptr<const CCNxName> name = packet->GetMessage ()->GetName ();
          if (FindOutStandingInterest (name))
            {
              NS_LOG_INFO (
                "CCNxMonitor:Received content back for Node " << GetNode ()->GetId () << *name);
              RemoveOutStandingInterest (name);

              // XXX: store the name-to-index mapping somewhere else

              // We previously sent an interest for this. Find out which one it is for.
              int probeIndex = m_probeIndexes[*name];
              Ptr<PacketProbe> probe = m_probes[probeIndex];
              int index = probe->m_count - 1;
              if (index < 0) {
                  break;
              }

              if (probe->m_hitName[index] != NULL && name->Equals(*probe->m_hitName[index])) {
                  probe->m_hitTime = receiveTime;
                  probe->m_hitWaiting = false;
                  probe->m_hits[probe->m_count - 1] = true;
              }

              if (probe->m_missName[index] != NULL && name->Equals(*probe->m_missName[index])) {
                  probe->m_missTime = receiveTime;
                  probe->m_missWaiting = false;
                  probe->m_misses[probe->m_count - 1] = true;
              }

              if (!probe->m_hitWaiting && !probe->m_missWaiting) {
                  // Bump up the count as needed
                  // XXX: pass in epsilon as a parameter
                  int epsilon = 5;
                  if (probe->IsCacheHit(epsilon)) {
                      m_countMap[probe->m_index]++;
                  }

                  // Sleep until we can probe for this packet again
                  // XXX: pass in t_c as a parameter
                  double t_c = 1.0;
                  Simulator::Schedule(Seconds(t_c), &CCNxMonitor::GenerateTraffic, this);
              }
            }
          else
            {
              m_contentProcessFails++;
              NS_LOG_ERROR (
                "CCNxMonitor:Received wrong content back for Node " << GetNode ()->GetId () << *name);
            }
        }
      else
        {
          m_contentProcessFails++;
          NS_LOG_ERROR ("CCNxMonitor:Bad packet type received " << *packet);
        }
    }
}

std::string
generateRandomString(const int len)
{
    std::string result;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        result = result + (alphanum[rand() % (sizeof(alphanum) - 1)]);
    }

    return result;
}

void
CCNxMonitor::ProbeTimerCallback (int index, bool hitInterest)
{
    Ptr<PacketProbe> probe = m_probes[index];
    int i = probe->m_count - 1;
    if (hitInterest && !probe->m_hits[i]) {
        this->SendInterestForName(probe->m_hitName[i], index, hitInterest);
    } else if (!hitInterest && !probe->m_misses[i]) {
        this->SendInterestForName(probe->m_missName[i], index, hitInterest);
    }
}


void
CCNxMonitor::SendInterestForName (Ptr<CCNxName> name, int index, bool probe)
{
    Ptr<CCNxInterest> interest = Create<CCNxInterest> (name);
    Ptr<CCNxPacket> packet = CCNxPacket::CreateFromMessage (interest);
    m_consumerPortal->Send (packet);
    m_goodInterestsSent++;

    // Schedule timer callback
    Simulator::Schedule(Seconds(1), &CCNxMonitor::ProbeTimerCallback, this, index, probe);
    NS_LOG_DEBUG ("CCNxMonitor:Sending interest request" << *packet << " packet dump");
}


void
CCNxMonitor::GenerateTraffic ()
{
  NS_LOG_FUNCTION_NOARGS ();

  // XXX: need to walk this entire list and try to generate each one that is not pending!
  Ptr<const CCNxName> name = m_globalContentRepositoryPrefix->GetNameAtIndex (m_count);
  if (name)
    {
      // Build the random segment
      std::string suffix = generateRandomString(32);
      Ptr<CCNxNameSegment> suffixSegment = Create<CCNxNameSegment>(CCNxNameSegment_Name, suffix);

      // Build the probe interest names
      int probeIndex = m_count % m_globalContentRepositoryPrefix->GetContentObjectCount();
      Ptr<PacketProbe> probe = m_probes[probeIndex];

      if (probe->m_hitWaiting || probe->m_missWaiting) {
          // If there's an outstanding query, dont' send another one
          return;
      }

      probe->m_hitName[probe->m_count] = Create<CCNxName>(*name);
      probe->m_missName[probe->m_count] = Create<CCNxName>(*name);
      probe->m_missName[probe->m_count]->AppendSegment(suffixSegment);

      // Stamp this packet probe
      Time sendTime = Simulator::Now ();
      probe->m_sendTime = sendTime;
      probe->m_hitTime = sendTime;
      probe->m_missTime = sendTime;
      probe->m_hitWaiting = true;
      probe->m_missWaiting = true;

      // Send both interests
      this->SendInterestForName(probe->m_hitName[probe->m_count], probeIndex, true);
      InsertOutStandingInterest (probe->m_hitName[probe->m_count]);
      m_probeIndexes[*probe->m_hitName[probe->m_count]] = probeIndex;
      this->SendInterestForName(probe->m_missName[probe->m_count], probeIndex, false);
      InsertOutStandingInterest (probe->m_missName[probe->m_count]);
      m_probeIndexes[*probe->m_missName[probe->m_count]] = probeIndex;

      // Bump the send counts
      probe->m_count++;
      m_count++;
    }
  else
    {
      NS_LOG_ERROR ("Bad Interest Generated");
      m_interestProcessFails++;
    }
}

std::vector<int>
CCNxMonitor::GetObservedHistogram ()
{
    std::vector<int> pop;

    for (int i = 0; i < m_globalContentRepositoryPrefix->GetContentObjectCount(); i++) {
        pop.push_back(m_countMap[i]);
    }

    return pop;
}

void
CCNxMonitor::ShowStatistics ()
{

  Ptr <Node> node = CCNxMonitor::GetNode ();
  if (printConsStatsHeader)
    {
      std::cout << std::endl <<  "Consumer " << " Interest " << "Content   " << \
    		  " Missing   " << " Bad        " << "Average   " << "Std Dev    " \
			  << "Total    " <<" Repository" << std::endl;
      std::cout << "Node Id :" << " Sent    :" << "Received  :" \
    		  << "Interests :" << "Packets   :" << "Delay(Ms):" <<  "Delay(Ms) :" \
			  << "Count     :" << "Prefix   " << std::endl;
      printConsStatsHeader = 0;
    }
  double average = 0.0;
  double stdev = 0.0;

  if (m_count > 0) {
	  average = m_sum / m_count;

	  // This comes from https://en.wikipedia.org/wiki/Standard_deviation#Identities_and_mathematical_properties
	  if (m_count > 2) {
		  double k = sqrt(m_count / (m_count - 1));

		  double variance = (m_sumSquare / m_count) - (average * average);
		  stdev = k * sqrt(variance);
	  }
  }
  std::cout << std::setw (10) << std::left << node->GetId ();
  std::cout << std::setw (10) << std::left  << m_goodInterestsSent;
  std::cout << std::setw (12) << std::left << m_goodContentReceived;
  std::cout << std::setw (12) << std::left << m_goodInterestsSent - m_goodContentReceived;
  std::cout << std::setw (8) << std::left << m_contentProcessFails + m_interestProcessFails;
  std::cout << std::setw (10) << std::left << average;
  std::cout << std::setw (11) << std::left << stdev;
  std::cout << std::setw (10) << std::left << m_count;
  std::cout << *m_globalContentRepositoryPrefix->GetRepositoryPrefix () << std::endl;

}
