#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/error-model.h"
#include "ns3/applications-module.h"

#include <map>
#include <fstream>
#include <iomanip>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWestwoodPlusSimulation");

std::map<uint32_t, std::ofstream> cwndStreams;
std::map<uint32_t, std::ofstream> ssthreshStreams;
std::map<uint32_t, std::ofstream> rttStreams;
std::map<uint32_t, std::ofstream> rtoStreams;
std::map<uint32_t, std::ofstream> inflightStreams;

// Tracing functions
void
CwndTracer (uint32_t nodeId, uint32_t oldCwnd, uint32_t newCwnd)
{
  auto &stream = cwndStreams[nodeId];
  stream << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

void
SsThreshTracer (uint32_t nodeId, uint32_t oldSsthresh, uint32_t newSsthresh)
{
  auto &stream = ssthreshStreams[nodeId];
  stream << Simulator::Now ().GetSeconds () << " " << newSsthresh << std::endl;
}

void
RttTracer (uint32_t nodeId, Time oldRtt, Time newRtt)
{
  auto &stream = rttStreams[nodeId];
  stream << Simulator::Now ().GetSeconds () << " " << newRtt.GetSeconds () << std::endl;
}

void
RtoTracer (uint32_t nodeId, Time oldRto, Time newRto)
{
  auto &stream = rtoStreams[nodeId];
  stream << Simulator::Now ().GetSeconds () << " " << newRto.GetSeconds () << std::endl;
}

void
InflightTracer (Ptr<const TcpSocketBase> socket, uint32_t nodeId)
{
  auto inflight = socket->GetBytesInFlight ();
  inflightStreams[nodeId] << Simulator::Now ().GetSeconds () << " " << inflight << std::endl;
}

void
InstallTcpTrace (Ptr<Node> node, Ptr<Socket> socket, uint32_t nodeId)
{
  Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase> (socket);
  if (!tcpSocket)
    return;

  // Congestion Window
  std::string cwndFilename = "cwnd-trace-node" + std::to_string (nodeId) + ".dat";
  cwndStreams[nodeId].open (cwndFilename, std::ios::out);
  tcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndTracer, nodeId));

  // Slow Start Threshold
  std::string ssthreshFilename = "ssthresh-trace-node" + std::to_string (nodeId) + ".dat";
  ssthreshStreams[nodeId].open (ssthreshFilename, std::ios::out);
  tcpSocket->TraceConnectWithoutContext ("SlowStartThreshold", MakeBoundCallback (&SsThreshTracer, nodeId));

  // RTT
  std::string rttFilename = "rtt-trace-node" + std::to_string (nodeId) + ".dat";
  rttStreams[nodeId].open (rttFilename, std::ios::out);
  tcpSocket->TraceConnectWithoutContext ("RTT", MakeBoundCallback (&RttTracer, nodeId));

  // RTO
  std::string rtoFilename = "rto-trace-node" + std::to_string (nodeId) + ".dat";
  rtoStreams[nodeId].open (rtoFilename, std::ios::out);
  tcpSocket->TraceConnectWithoutContext ("RTO", MakeBoundCallback (&RtoTracer, nodeId));

  std::string inflightFilename = "inflight-trace-node" + std::to_string (nodeId) + ".dat";
  inflightStreams[nodeId].open (inflightFilename, std::ios::out);
  // Schedule logging in-flight every 0.01s
  Simulator::ScheduleNow ([tcpSocket, nodeId] () mutable {
    if (Simulator::Now ().GetSeconds () > 0)
      InflightTracer (tcpSocket, nodeId);
    Simulator::Schedule (MilliSeconds (10), [tcpSocket, nodeId] () mutable {
      if (!tcpSocket->IsClosed ())
        InflightTracer (tcpSocket, nodeId);
    });
  });
}

// Helper: Set up TCP socket and tracing after application creation
void
SetupSocketTracing (Ptr<Node> node, uint32_t appIndex, uint32_t nodeId)
{
  Ptr<Application> app = node->GetApplication (appIndex);
  if (!app)
    return;
  Ptr<BulkSendApplication> bulkApp = DynamicCast<BulkSendApplication> (app);
  if (bulkApp)
    {
      Ptr<Socket> socket = bulkApp->GetSocket ();
      if (socket)
        InstallTcpTrace (node, socket, nodeId);
    }
}

void
ConfigureTcp (std::string variant)
{
  if (variant == "WestwoodPlus")
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpWestwoodPlus"));
    }
  else if (variant == "Westwood")
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpWestwood"));
    }
  else
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
    }
}

// Command line arguments default values
std::string bandwidth = "10Mbps";
std::string delay = "50ms";
double per = 0.0;
uint32_t flows = 2;
std::string tcpVariant = "WestwoodPlus";
uint32_t maxBytes = 0;
uint32_t simTime = 20;

void
ParseCommandLine (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("bandwidth", "Link Bandwidth", bandwidth);
  cmd.AddValue ("delay", "Link delay", delay);
  cmd.AddValue ("per", "Packet error rate", per);
  cmd.AddValue ("flows", "Number of TCP flows", flows);
  cmd.AddValue ("tcpVariant", "TCP Variant (WestwoodPlus/Westwood/NewReno)", tcpVariant);
  cmd.AddValue ("maxBytes", "Max bytes per BulkSendApp (0=unlimited)", maxBytes);
  cmd.AddValue ("simTime", "Simulation Time (seconds)", simTime);
  cmd.Parse (argc, argv);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("TcpWestwoodPlusSimulation", LOG_LEVEL_INFO);

  ParseCommandLine (argc, argv);
  ConfigureTcp (tcpVariant);

  NodeContainer nodes;
  nodes.Create (2 + flows * 2); // n0---R---n1, each flow has sender/receiver behind each

  // Point-to-point topology
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  p2p.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;

  // Router at nodes 0 and 1
  // For each flow: n_sender_i <-> n0 <==> n1 <-> n_receiver_i
  std::vector<NetDeviceContainer> flowDevices;
  std::vector<Ipv4InterfaceContainer> flowInterfaces;
  std::vector<Ipv4Address> sinkAddresses;

  // Left branches (sender-to-router), index: [2i, 2i+1]
  for (uint32_t i = 0; i < flows; ++i)
    {
      NodeContainer s2r ({2 + 2 * i, 0}); // sender <-> router 0
      flowDevices.push_back (p2p.Install (s2r));
    }

  // Core link (router0 <-> router1)
  NetDeviceContainer coreDevices = p2p.Install (NodeContainer (0, 1));

  // Right branches (router-to-receiver)
  for (uint32_t i = 0; i < flows; ++i)
    {
      NodeContainer r2d ({1, 2 + 2 * i + 1}); // router 1 <-> receiver
      flowDevices.push_back (p2p.Install (r2d));
    }

  // Install Internet stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4Address> senderIpAddrs;
  uint32_t subnet = 1;

  // Sender-to-router links
  for (uint32_t i = 0; i < flows; ++i, ++subnet)
    {
      std::ostringstream subnetStr;
      subnetStr << "10." << subnet << ".1.0";
      address.SetBase (Ipv4Address (subnetStr.str ().c_str ()), Ipv4Mask ("255.255.255.0"));
      flowInterfaces.push_back (address.Assign (flowDevices[i]));
      senderIpAddrs.push_back (flowInterfaces.back ().GetAddress (0));
    }

  // Router0-router1 core link
  {
    std::ostringstream subnetStr;
    subnetStr << "10." << ++subnet << ".1.0";
    address.SetBase (Ipv4Address (subnetStr.str ().c_str ()), Ipv4Mask ("255.255.255.0"));
    interfaces = address.Assign (coreDevices);
  }

  // Router-to-dest links
  for (uint32_t i = 0; i < flows; ++i, ++subnet)
    {
      std::ostringstream subnetStr;
      subnetStr << "10." << subnet << ".1.0";
      address.SetBase (Ipv4Address (subnetStr.str ().c_str ()), Ipv4Mask ("255.255.255.0"));
      flowInterfaces.push_back (address.Assign (flowDevices[flows + i]));
      sinkAddresses.push_back (flowInterfaces.back ().GetAddress (1));
    }

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Error Model (on core link)
  if (per > 0.0)
    {
      Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
      em->SetAttribute ("ErrorRate", DoubleValue (per));
      coreDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }

  // Install applications and tracing
  ApplicationContainer sinkApps, sourceApps;
  uint16_t portBase = 50000;
  for (uint32_t i = 0; i < flows; ++i)
    {
      // Sink on receiver node
      PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), portBase + i));
      ApplicationContainer sinkApp = sink.Install (nodes.Get (2 + 2 * i + 1));
      sinkApp.Start (Seconds (0));
      sinkApp.Stop (Seconds (simTime + 1));
      sinkApps.Add (sinkApp);

      // BulkSend on sender node
      BulkSendHelper bulk ("ns3::TcpSocketFactory", InetSocketAddress (sinkAddresses[i], portBase + i));
      bulk.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
      ApplicationContainer srcApp = bulk.Install (nodes.Get (2 + 2 * i));
      srcApp.Start (Seconds (1));
      srcApp.Stop (Seconds (simTime));
      sourceApps.Add (srcApp);
    }

  Simulator::Schedule (Seconds (1.2), [&] (){
    for (uint32_t i = 0; i < flows; ++i)
      {
        SetupSocketTracing (nodes.Get (2 + 2 * i), 0, i);
      }
  });

  Simulator::Stop (Seconds (simTime + 2));
  Simulator::Run ();
  Simulator::Destroy ();

  for (auto &kv : cwndStreams) kv.second.close ();
  for (auto &kv : ssthreshStreams) kv.second.close ();
  for (auto &kv : rttStreams) kv.second.close ();
  for (auto &kv : rtoStreams) kv.second.close ();
  for (auto &kv : inflightStreams) kv.second.close ();

  return 0;
}