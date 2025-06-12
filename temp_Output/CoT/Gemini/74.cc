#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingComparison");

int main (int argc, char *argv[])
{
  bool enableMobilityTrace = false;
  bool enableFlowMonitor = false;
  std::string routingProtocol = "AODV";
  double simulationTime = 200;
  uint32_t numNodes = 50;
  double maxSpeed = 20;
  double txPowerDbm = 7.5;
  double maxX = 300;
  double maxY = 1500;

  CommandLine cmd;
  cmd.AddValue ("routingProtocol", "Routing Protocol [AODV|DSDV|OLSR|DSR]", routingProtocol);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("maxSpeed", "Max speed of nodes in m/s", maxSpeed);
  cmd.AddValue ("txPowerDbm", "Tx Power in dBm", txPowerDbm);
  cmd.AddValue ("maxX", "X dimension of the bounding box", maxX);
  cmd.AddValue ("maxY", "Y dimension of the bounding box", maxY);
  cmd.AddValue ("enableMobilityTrace", "Enable mobility trace", enableMobilityTrace);
  cmd.AddValue ("enableFlowMonitor", "Enable flow monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue (10000));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", EnumValue (WifiRemoteStationManager::NonUnicastMode::OFDM_RATE_9));

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(maxX) + "]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(maxY) + "]"),
                                 "Z", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(maxSpeed) + "]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  if (routingProtocol == "OLSR")
  {
    OlsrHelper olsr;
    internet.AddProtocol (&olsr, 10);
  }
  else if (routingProtocol == "DSDV")
  {
    DsdvHelper dsdv;
    internet.AddProtocol (&dsdv, 10);
  }
  else if (routingProtocol == "AODV")
  {
    AodvHelper aodv;
    internet.AddProtocol (&aodv, 10);
  }
  else if (routingProtocol == "DSR")
  {
      DsrHelper dsr;
      DsrMainRoutingHelper dsrRoutingHelper;
      internet.AddProtocol (&dsr, 10);
      internet.SetRoutingHelper (dsrRoutingHelper);
  }
  else
  {
    std::cerr << "Error: Invalid routing protocol " << routingProtocol << std::endl;
    return 1;
  }

  uint16_t port = 9;

  for (uint32_t i = 0; i < 10; ++i)
  {
    uint32_t srcNode = i % numNodes;
    uint32_t dstNode = (i + 10) % numNodes;

    UdpEchoServerHelper echoServer (port);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (dstNode));
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simulationTime));

    UdpEchoClientHelper echoClient (interfaces.GetAddress (dstNode, 0), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (srcNode));
    clientApps.Start (Seconds (50.0 + (double)i / 10.0));
    clientApps.Stop (Seconds (simulationTime));
  }

  Simulator::Stop (Seconds (simulationTime));

  if (enableMobilityTrace)
  {
    std::ofstream asciiTraceFile;
    asciiTraceFile.open ("mobility.txt");
    MobilityModel::SetMobilityTraceFile ("mobility.txt");
  }

  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
  {
    flowmon.InstallAll ();
  }

  Simulator::Run ();

  if (enableFlowMonitor)
  {
    flowmon.SerializeToXmlFile ("manet_routing_comparison.flowmon", false, true);
  }

  std::ofstream output ("manet_routing_results.csv");
  output << "Source Address,Destination Address,Packets Sent,Packets Received,Packet Loss Rate" << std::endl;

  if (enableFlowMonitor) {
    Ptr<FlowMonitor> monitor = flowmon.GetMonitor ();
    for (auto iter = monitor->GetFlowStats().begin(); iter != monitor->GetFlowStats().end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = iter->second.fiveTuple;
        output << t.sourceAddress << "," << t.destinationAddress << "," << iter->second.packetsSent << ","
               << iter->second.packetsReceived << ","
               << 1.0 - (double)iter->second.packetsReceived / (double)iter->second.packetsSent << std::endl;
    }
  }

  output.close();

  Simulator::Destroy ();
  return 0;
}