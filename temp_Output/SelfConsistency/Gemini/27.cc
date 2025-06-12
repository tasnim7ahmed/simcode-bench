#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiGoodput");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWifi = 1;
  double simulationTime = 10;
  std::string rate ("5Mbps");
  double distance = 10.0;
  bool enableRtsCts = false;
  std::string protocol = "UdpClient"; // Or "TcpClient"
  uint32_t packetSize = 1472; // bytes
  uint32_t port = 9;
  double expectedThroughput = 5; // Mbps

  // Wifi parameters
  int mcs = 0;
  bool shortGuardInterval = true;
  int channelWidth = 20;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell applications to log if true", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("rate", "CBR traffic rate", rate);
  cmd.AddValue ("distance", "Distance between nodes", distance);
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
  cmd.AddValue ("protocol", "Transport protocol to use: UdpClient or TcpClient", protocol);
  cmd.AddValue ("packetSize", "Size of UDP packets to send", packetSize);
  cmd.AddValue ("port", "Port number for the traffic", port);
  cmd.AddValue ("expectedThroughput", "Expected throughput (Mbps)", expectedThroughput);

  cmd.AddValue ("mcs", "MCS value (0-7)", mcs);
  cmd.AddValue ("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
  cmd.AddValue ("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
    }

  NodeContainer staNodes;
  staNodes.Create (nWifi);
  NodeContainer apNodes;
  apNodes.Create (1);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Configure channel width
  Config::SetDefault ("ns3::WifiChannel::Frequency", UintegerValue (2412)); // Set a valid frequency
  if (channelWidth == 40)
  {
    Config::SetDefault ("ns3::WifiChannel::ChannelWidth", UintegerValue (40));
  }
  else
  {
    Config::SetDefault ("ns3::WifiChannel::ChannelWidth", UintegerValue (20));
  }

  // Configure guard interval
  if (shortGuardInterval)
  {
    Config::SetDefault ("ns3::WifiMacQueue::ShortGuardInterval", BooleanValue (true));
  }
  else
  {
    Config::SetDefault ("ns3::WifiMacQueue::ShortGuardInterval", BooleanValue (false));
  }


  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (wifiPhy, wifiMac, staNodes);

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconInterval", TimeValue (Seconds (0.05)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (wifiPhy, wifiMac, apNodes);

  // Set the MCS
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxBeamformingSupported", BooleanValue (true));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue (true));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PreambleDetectionTimeout", TimeValue (MicroSeconds (10)));

  std::stringstream ss;
  ss << "HtMcs" << mcs;
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/MaxSupportedTxMcs", StringValue (ss.str ()));


  if (enableRtsCts)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue (0));
    }

  // Mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (apNodes);

  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=3.0]"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);

  // Set positions based on distance
  Ptr<Node> apNode = apNodes.Get (0);
  Ptr<Node> staNode = staNodes.Get (0);
  Vector3D position (distance, 0, 0);
  staNode->GetObject<MobilityModel> ()->SetPosition (position);
  apNode->GetObject<MobilityModel> ()->SetPosition (Vector3D (0, 0, 0));

  // Internet stack
  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  Ipv4InterfaceContainer apNodeInterface;
  staNodeInterface = address.Assign (staDevices);
  apNodeInterface = address.Assign (apDevices);

  // Applications
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;

  if (protocol == "UdpClient") {
    UdpServerHelper server (port);
    serverApps = server.Install (apNodes);
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simulationTime + 1));

    UdpClientHelper client (apNodeInterface.GetAddress (0), port);
    client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
    client.SetAttribute ("Interval", TimeValue (Time (Seconds (0.000008)))); // Adjust to achieve desired rate
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    clientApps = client.Install (staNodes);
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simulationTime));

  } else if (protocol == "TcpClient") {

    PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    serverApps = sink.Install (apNodes.Get(0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simulationTime + 1));

    BulkSendHelper client ("ns3::TcpSocketFactory", InetSocketAddress (apNodeInterface.GetAddress (0), port));
    client.SetAttribute ("MaxBytes", UintegerValue (0)); // Unlimited data
    client.SetAttribute ("SendSize", UintegerValue (packetSize));
    clientApps = client.Install (staNodes);
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simulationTime));
  }
  else {
        std::cerr << "Error: Invalid protocol specified.  Use UdpClient or TcpClient." << std::endl;
        return 1;
  }


  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 2));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  double goodput = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apNodeInterface.GetAddress (0))
        {
          goodput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000;
          std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  Throughput: " << goodput << " Mbps\n";
          std::cout << "  Expected Throughput: " << expectedThroughput << " Mbps\n";

          if (goodput >= expectedThroughput)
            {
              std::cout << "*** Goodput is as expected! ***\n";
            }
          else
            {
              std::cout << "*** Goodput is less than expected. ***\n";
            }
          break;
        }
    }

  monitor->SerializeToXmlFile("wifi_goodput.xml", true, true);

  Simulator::Destroy ();

  return 0;
}