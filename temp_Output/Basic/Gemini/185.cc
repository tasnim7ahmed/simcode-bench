#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiNetwork");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns3-wifi")));

  NodeContainer apNodes;
  apNodes.Create (3);

  NodeContainer staNodes;
  staNodes.Create (6);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, apNodes);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (staNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);
  apNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0,0.0,0.0));
  apNodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(20.0,0.0,0.0));
  apNodes.Get(2)->GetObject<MobilityModel>()->SetPosition(Vector(40.0,0.0,0.0));

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  UdpClientServerHelper echoClient (9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (500));
  echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  clientApps.Add (echoClient.Install (staNodes.Get (0)));
  clientApps.Add (echoClient.Install (staNodes.Get (1)));
   clientApps.Add (echoClient.Install (staNodes.Get (2)));
  clientApps.Add (echoClient.Install (staNodes.Get (3)));
   clientApps.Add (echoClient.Install (staNodes.Get (4)));
  clientApps.Add (echoClient.Install (staNodes.Get (5)));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  AnimationInterface anim ("wifi-network.xml");
  anim.SetConstantPosition (apNodes.Get (0), 0.0, 10.0);
  anim.SetConstantPosition (apNodes.Get (1), 20.0, 10.0);
  anim.SetConstantPosition (apNodes.Get (2), 40.0, 10.0);
  for (uint32_t i = 0; i < staNodes.GetN (); ++i)
  {
    anim.UpdateNodeDescription (staNodes.Get (i), "STA");
  }
  for (uint32_t i = 0; i < apNodes.GetN (); ++i)
  {
    anim.UpdateNodeDescription (apNodes.Get (i), "AP");
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  if (tracing)
    {
      phy.EnablePcapAll ("wifi-network");
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
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("wifi-network.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}