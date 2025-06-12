#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/ssid.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHandoverSimulation");

int main (int argc, char *argv[])
{
  bool enableNetAnim = true;
  double simulationTime = 100;
  uint32_t numberOfNodes = 6;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("enableNetAnim", "Enable NetAnim visualization", enableNetAnim);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("numberOfNodes", "Number of mobile nodes", numberOfNodes);
  cmd.Parse (argc, argv);

  NodeContainer apNodes;
  apNodes.Create (2);

  NodeContainer mobileNodes;
  mobileNodes.Create (numberOfNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns-3-ssid-1");
  Ssid ssid2 = Ssid ("ns-3-ssid-2");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices1 = wifi.Install (phy, mac, apNodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices2 = wifi.Install (phy, mac, apNodes.Get (1));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, mobileNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)));
  mobility.Install (mobileNodes);
  mobility.Install (apNodes);

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (mobileNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (apNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpEchoClientHelper echoClient (apInterfaces1.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (mobileNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  if (enableNetAnim)
    {
      AnimationInterface anim ("wifi-handover.xml");
      anim.SetConstantPosition (apNodes.Get (0), 10, 10);
      anim.SetConstantPosition (apNodes.Get (1), 40, 40);
    }

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024  << " kbps\n";
    }

  monitor->SerializeToXmlFile("wifi-handover.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}