#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocGridWifiOlsr");

int main (int argc, char *argv[])
{
  uint32_t gridWidth = 5;
  uint32_t gridHeight = 5;
  double distance = 50.0;
  std::string phyMode = "DsssRate11Mbps";
  uint32_t packetSize = 1000;
  uint32_t numPackets = 1;
  uint32_t sourceNodeId = 24;
  uint32_t sinkNodeId = 0;
  double simulationTime = 10.0;
  bool verbose = false;
  bool enablePcap = false;
  bool enableAscii = false;

  CommandLine cmd;
  cmd.AddValue ("phyMode", "Wifi Physical layer mode (e.g., DsssRate1Mbps, DsssRate11Mbps)", phyMode);
  cmd.AddValue ("distance", "Grid step distance in meters", distance);
  cmd.AddValue ("packetSize", "Size of application packets (bytes)", packetSize);
  cmd.AddValue ("numPackets", "Total number of packets", numPackets);
  cmd.AddValue ("sourceNodeId", "Node ID of packet source", sourceNodeId);
  cmd.AddValue ("sinkNodeId", "Node ID of packet sink", sinkNodeId);
  cmd.AddValue ("gridWidth", "Number of grid columns", gridWidth);
  cmd.AddValue ("gridHeight", "Number of grid rows", gridHeight);
  cmd.AddValue ("verbose", "Enable application and OLSR logging", verbose);
  cmd.AddValue ("pcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("ascii", "Enable ASCII tracing", enableAscii);
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.Parse (argc, argv);

  uint32_t nNodes = gridWidth * gridHeight;
  if (sourceNodeId >= nNodes) sourceNodeId = nNodes - 1;
  if (sinkNodeId >= nNodes) sinkNodeId = 0;

  if (verbose)
    {
      LogComponentEnable ("AdhocGridWifiOlsr", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("OlsrRoutingProtocol", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (nNodes);

  // Physical/MAC configuration
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0206)); // ~40mW, typical 802.11b
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0206));   // Set same TxPower for all

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  // Propagation loss settings to force distance-based RSSI cutoff
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue (distance * 2.5)); // Cutoff at 2.5 x grid step

  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (phyMode),
                               "ControlMode", StringValue (phyMode));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (distance),
                                "DeltaY", DoubleValue (distance),
                                "GridWidth", UintegerValue (gridWidth),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Stack and Routing
  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Applications
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (sinkNodeId));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (sinkNodeId), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (sourceNodeId));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  // Tracing
  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("adhoc-grid-olsr", false);
    }
  if (enableAscii)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("adhoc-grid-olsr.tr"));
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}