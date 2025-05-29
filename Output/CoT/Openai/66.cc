#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/error-model.h"

#include <map>
#include <fstream>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWestwoodPlusSim");

std::map<uint32_t, std::ofstream> cwndStreamMap;
std::map<uint32_t, std::ofstream> ssthreshStreamMap;
std::map<uint32_t, std::ofstream> rttStreamMap;
std::map<uint32_t, std::ofstream> rtoStreamMap;
std::map<uint32_t, std::ofstream> inflightStreamMap;

void
CwndTracer (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

void
SsthreshTracer (Ptr<OutputStreamWrapper> stream, uint32_t oldSsthresh, uint32_t newSsthresh)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newSsthresh << std::endl;
}

void
RttTracer (Ptr<OutputStreamWrapper> stream, Time oldRtt, Time newRtt)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newRtt.GetMilliSeconds () << std::endl;
}

void
RtoTracer (Ptr<OutputStreamWrapper> stream, Time oldRto, Time newRto)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newRto.GetMilliSeconds () << std::endl;
}

void
InflightTracer (Ptr<OutputStreamWrapper> stream, uint32_t oldInflight, uint32_t newInflight)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newInflight << std::endl;
}

void
SetupSocketTracing (Ptr<Socket> socket, Ptr<Node> node, uint32_t flowId)
{
  std::ostringstream cwndName, ssthreshName, rttName, rtoName, inflightName;
  cwndName << "cwnd-flow" << flowId << ".dat";
  ssthreshName << "ssthresh-flow" << flowId << ".dat";
  rttName << "rtt-flow" << flowId << ".dat";
  rtoName << "rto-flow" << flowId << ".dat";
  inflightName << "inflight-flow" << flowId << ".dat";

  Ptr<OutputStreamWrapper> cwndStream = Create<OutputStreamWrapper> (cwndName.str (), std::ios::out);
  Ptr<OutputStreamWrapper> ssthreshStream = Create<OutputStreamWrapper> (ssthreshName.str (), std::ios::out);
  Ptr<OutputStreamWrapper> rttStream = Create<OutputStreamWrapper> (rttName.str (), std::ios::out);
  Ptr<OutputStreamWrapper> rtoStream = Create<OutputStreamWrapper> (rtoName.str (), std::ios::out);
  Ptr<OutputStreamWrapper> inflightStream = Create<OutputStreamWrapper> (inflightName.str (), std::ios::out);

  socket->TraceConnectWithoutContext (
    "CongestionWindow", MakeBoundCallback (&CwndTracer, cwndStream));
  socket->TraceConnectWithoutContext (
    "SlowStartThreshold", MakeBoundCallback (&SsthreshTracer, ssthreshStream));
  socket->TraceConnectWithoutContext (
    "RTT", MakeBoundCallback (&RttTracer, rttStream));
  socket->TraceConnectWithoutContext (
    "RTO", MakeBoundCallback (&RtoTracer, rtoStream));
  socket->TraceConnectWithoutContext (
    "BytesInFlight", MakeBoundCallback (&InflightTracer, inflightStream));
}

void
InstallTracingOnApp (Ptr<Application> app, Ptr<Node> node, uint32_t flowId)
{
  Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication> (app);
  if (bulk)
    {
      Ptr<Socket> socket = bulk->GetSocket ();
      SetupSocketTracing (socket, node, flowId);
      return;
    }
  Ptr<OnOffApplication> onoff = DynamicCast<OnOffApplication> (app);
  if (onoff)
    {
      Ptr<Socket> socket = onoff->GetSocket ();
      SetupSocketTracing (socket, node, flowId);
    }
}

NetDeviceContainer
CreatePointToPointLink (Ptr<Node> n1, Ptr<Node> n2,
                        std::string bandwidth, std::string delay, double per)
{
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  p2p.SetChannelAttribute ("Delay", StringValue (delay));
  NetDeviceContainer devices = p2p.Install (n1, n2);

  if (per > 0.0)
    {
      Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
      em->SetAttribute ("ErrorRate", DoubleValue (per));
      devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }
  return devices;
}

int
main (int argc, char *argv[])
{
  uint32_t flows = 2;
  std::string bandwidth = "10Mbps";
  std::string delay = "50ms";
  std::string tcpVariant = "TcpWestwoodPlus";
  double packetErrorRate = 0.0001;
  double simTime = 20.0;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("flows", "Number of TCP flows", flows);
  cmd.AddValue ("bandwidth", "Link bandwidth", bandwidth);
  cmd.AddValue ("delay", "Link delay", delay);
  cmd.AddValue ("tcpVariant", "TCP variant (TcpWestwoodPlus, TcpNewReno, etc.)", tcpVariant);
  cmd.AddValue ("packetErrorRate", "Packet error rate", packetErrorRate);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                      StringValue ("ns3::" + tcpVariant));

  NodeContainer nodes;
  nodes.Create (2 * flows);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces;

  for (uint32_t i = 0; i < flows; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      NetDeviceContainer devices = CreatePointToPointLink (
        nodes.Get (2 * i), nodes.Get (2 * i + 1), bandwidth, delay, packetErrorRate);
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaces.push_back (address.Assign (devices));
    }

  uint16_t port = 50000;
  ApplicationContainer apps;

  for (uint32_t i = 0; i < flows; ++i)
    {
      Address sinkAddress (InetSocketAddress (interfaces[i].GetAddress (1), port));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                   InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (2 * i + 1));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (simTime));

      BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", sinkAddress);
      sourceHelper.SetAttribute ("MaxBytes", UintegerValue (0));
      ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (2 * i));
      sourceApp.Start (Seconds (1.0 + i));
      sourceApp.Stop (Seconds (simTime));

      apps.Add (sourceApp);
      apps.Add (sinkApp);
    }

  for (uint32_t i = 0; i < flows; ++i)
    {
      Ptr<Application> srcApp = apps.Get (2 * i);
      Ptr<Node> srcNode = nodes.Get (2 * i);
      InstallTracingOnApp (srcApp, srcNode, i+1);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}