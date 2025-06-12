#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedWiredWireless");

int main (int argc, char *argv[])
{
  bool tracing = false;
  bool enableCourseChange = false;
  uint32_t nBackboneRouters = 3;
  uint32_t nLanNodes = 2;
  uint32_t nWifiNodes = 2;
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("enableCourseChange", "Enable course change callback", enableCourseChange);
  cmd.AddValue ("nBackboneRouters", "Number of backbone routers", nBackboneRouters);
  cmd.AddValue ("nLanNodes", "Number of LAN nodes per router", nLanNodes);
  cmd.AddValue ("nWifiNodes", "Number of WiFi nodes per router", nWifiNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  LogComponent::SetLevel (LogComponent::GetComponent ("UdpClient"), LOG_LEVEL_INFO);
  LogComponent::SetLevel (LogComponent::GetComponent ("UdpServer"), LOG_LEVEL_INFO);

  NodeContainer backboneRouters;
  backboneRouters.Create (nBackboneRouters);

  OlsrHelper olsrRouting;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsrRouting);
  internet.Install (backboneRouters);

  WifiHelper wifiAdhoc;
  wifiAdhoc.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifiAdhoc.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue ("DsssRate1Mbps"),
                                     "ControlMode", StringValue ("DsssRate1Mbps"));

  NqosWifiMacHelper macAdhoc = NqosWifiMacHelper::Default ();
  macAdhoc.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper phyAdhoc = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channelAdhoc = YansWifiChannelHelper::Create ();
  phyAdhoc.SetChannel (channelAdhoc.Create ());
  NetDeviceContainer adhocDevices = wifiAdhoc.Install (phyAdhoc, macAdhoc, backboneRouters);

  MobilityHelper mobilityAdhoc;
  mobilityAdhoc.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                      "X", StringValue ("50.0"),
                                      "Y", StringValue ("50.0"),
                                      "Z", StringValue ("0.0"),
                                      "Rho", StringValue ("10.0"));
  mobilityAdhoc.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                   "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  mobilityAdhoc.Install (backboneRouters);

  if (enableCourseChange)
  {
    for (uint32_t i = 0; i < backboneRouters.GetN (); ++i)
    {
      Ptr<MobilityModel> mob = backboneRouters.Get (i)->GetObject<MobilityModel> ();
      mob->TraceConnectWithoutContext ("CourseChange", MakeCallback (&(void (*) (std::string))[](std::string str){
        std::cout << "CourseChange " << str << std::endl;
      }));
    }
  }


  NodeContainer lanNodes[nBackboneRouters];
  NodeContainer wifiNodes[nBackboneRouters];
  NetDeviceContainer lanDevices[nBackboneRouters];
  NetDeviceContainer wifiStaDevices[nBackboneRouters];
  NetDeviceContainer wifiApDevices[nBackboneRouters];

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  WifiHelper wifiInfra;
  wifiInfra.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifiInfra.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue ("DsssRate1Mbps"),
                                      "ControlMode", StringValue ("DsssRate1Mbps"));

  NqosWifiMacHelper macInfra = NqosWifiMacHelper::Default ();
  Ssid ssid = Ssid ("ns3-wifi");
  macInfra.SetType ("ns3::StaWifiMac",
                    "Ssid", SsidValue (ssid),
                    "ActiveProbing", BooleanValue (false));

  YansWifiPhyHelper phyInfra = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channelInfra = YansWifiChannelHelper::Create ();
  phyInfra.SetChannel (channelInfra.Create ());

  WifiMacHelper macAp;
  macAp.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));


  for (uint32_t i = 0; i < nBackboneRouters; ++i)
  {
    lanNodes[i].Create (nLanNodes);
    internet.Install (lanNodes[i]);

    wifiNodes[i].Create (nWifiNodes);
    internet.Install (wifiNodes[i]);

    lanDevices[i] = p2p.Install (backboneRouters.Get (i), lanNodes[i]);

    wifiStaDevices[i] = wifiInfra.Install (phyInfra, macInfra, wifiNodes[i]);
    wifiApDevices[i] = wifiInfra.Install (phyInfra, macAp, backboneRouters.Get (i));
  }


  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces = address.Assign (adhocDevices);

  for (uint32_t i = 0; i < nBackboneRouters; ++i)
  {
    address.SetBase (("10.1." + std::to_string(i + 2) + ".0").c_str(), "255.255.255.0");
    Ipv4InterfaceContainer lanInterfaces = address.Assign (lanDevices[i]);

    address.SetBase (("10.1." + std::to_string(i + 2 + nBackboneRouters) + ".0").c_str(), "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign (wifiStaDevices[i]);
    Ipv4InterfaceContainer apInterface = address.Assign (wifiApDevices[i]);
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (wifiNodes[nBackboneRouters - 1].Get (nWifiNodes - 1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpClientHelper echoClient (wifiNodes[nBackboneRouters - 1].Get (nWifiNodes - 1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (lanNodes[0].Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));


  if (tracing)
    {
      phyAdhoc.EnablePcapAll ("adhoc");
      p2p.EnablePcapAll ("lan");
      phyInfra.EnablePcapAll ("wifi");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  AnimationInterface anim ("mixed-wired-wireless.xml");
  for (uint32_t i = 0; i < nBackboneRouters; ++i)
  {
    anim.SetConstantPosition (backboneRouters.Get(i), 50 + i * 10, 50);
    for (uint32_t j = 0; j < nLanNodes; ++j)
    {
      anim.SetConstantPosition (lanNodes[i].Get(j), 50 + i * 10, 30 + j * 5);
    }
    for (uint32_t j = 0; j < nWifiNodes; ++j)
    {
      anim.SetConstantPosition (wifiNodes[i].Get(j), 50 + i * 10, 70 + j * 5);
    }
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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

  Simulator::Destroy ();

  return 0;
}