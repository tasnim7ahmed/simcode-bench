#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTwoApHandoverExample");

// Custom tracing of association events
static uint32_t
handoverCount = 0;

void
AssocTrace (Mac48Address oldAp, Mac48Address newAp, Mac48Address sta)
{
  ++handoverCount;
  std::cout << "Time " << Simulator::Now ().GetSeconds ()
            << "s: STA (" << sta << ") switches from AP ("
            << oldAp << ") to AP (" << newAp << ")" << std::endl;
}

int
main (int argc, char *argv[])
{
  uint32_t nWifi = 6;
  double simTime = 30.0;
  double areaWidth = 100.0;
  double areaHeight = 50.0;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Setup AP Nodes
  NodeContainer apNodes;
  apNodes.Create (2);

  // Setup STA Nodes
  NodeContainer staNodes;
  staNodes.Create (nWifi);

  // Channel and PHY
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  // SSIDs for both APs
  Ssid ssidA = Ssid ("AP1-network");
  Ssid ssidB = Ssid ("AP2-network");

  WifiMacHelper mac;
  NetDeviceContainer apDevices;
  NetDeviceContainer staDevices;
  NodeContainer allNodes;
  allNodes.Add (apNodes);
  allNodes.Add (staNodes);

  // Install AP devices
  for (uint32_t i = 0; i < 2; ++i)
    {
      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue ((i == 0) ? ssidA : ssidB));
      apDevices.Add (wifi.Install (phy, mac, apNodes.Get (i)));
    }

  // Install STA devices (each can associate to any AP)
  mac.SetType ("ns3::StaWifiMac",
               "ActiveProbing", BooleanValue (true));
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      staDevices.Add (wifi.Install (phy, mac, staNodes.Get (i)));
    }

  // Mobility for APs (fixed)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (20.0, areaHeight / 2, 0.0)); // AP1
  positionAlloc->Add (Vector (80.0, areaHeight / 2, 0.0)); // AP2
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);

  // Mobility for STA nodes
  MobilityHelper staMobility;
  staMobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                    "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  staMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (0, areaWidth, 0, areaHeight)),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                                "Distance", DoubleValue (10.0));
  staMobility.Install (staNodes);

  // Connect association event trace hooks for all STA devices
  for (uint32_t i = 0; i < staDevices.GetN (); ++i)
    {
      Ptr<WifiNetDevice> wnd = DynamicCast<WifiNetDevice> (staDevices.Get (i));
      if (wnd)
        {
          Ptr<RegularWifiMac> mac = wnd->GetMac ()->GetObject<RegularWifiMac> ();
          mac->TraceConnectWithoutContext ("Roamed", MakeCallback (&AssocTrace));
        }
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  // Assign addresses for AP1
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> apInterfaces (2);
  std::vector<Ipv4InterfaceContainer> staInterfaces (2);

  // AP1 and associated STAs
  address.SetBase ("192.168.1.0", "255.255.255.0");
  apInterfaces[0] = address.Assign (apDevices.Get (0));
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      // All STAs use both subnets (roaming), but the initial address assignment for demonstration
      if (i < nWifi / 2)
        {
          staInterfaces[0].Add (address.Assign (staDevices.Get (i)));
        }
    }

  // AP2 and associated STAs
  address.SetBase ("192.168.2.0", "255.255.255.0");
  apInterfaces[1] = address.Assign (apDevices.Get (1));
  for (uint32_t i = nWifi / 2; i < nWifi; ++i)
    {
      staInterfaces[1].Add (address.Assign (staDevices.Get (i)));
    }

  // Applications: Each STA sends UDP to a server attached to AP1
  uint16_t port = 9000;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (apNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simTime));

  UdpClientHelper client (apInterfaces[0].GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.05)));
  client.SetAttribute ("PacketSize", UintegerValue (200));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      clientApps.Add (client.Install (staNodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simTime - 1.0));

  // Enable PCAP
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-two-ap-handover", apDevices);
  phy.EnablePcap ("wifi-two-ap-handover", staDevices);

  // Enable NetAnim
  AnimationInterface anim ("wifi-two-ap-handover.xml");
  anim.SetConstantPosition (apNodes.Get (0), 20.0, areaHeight / 2);
  anim.SetConstantPosition (apNodes.Get (1), 80.0, areaHeight / 2);
  for (uint32_t i = 0; i < nWifi; ++i)
    {
      Vector v = staNodes.Get (i)->GetObject<MobilityModel> ()->GetPosition ();
      anim.SetConstantPosition (staNodes.Get (i), v.x, v.y);
    }
  anim.UpdateNodeColor (apNodes.Get (0)->GetId (), 255, 0, 0); // AP1 red
  anim.UpdateNodeColor (apNodes.Get (1)->GetId (), 0, 0, 255); // AP2 blue

  // Enable tracing
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  uint64_t totalLost = monitor->GetLostPackets ();
  std::cout << "Total handover events: " << handoverCount << std::endl;
  std::cout << "Total packets lost across the network: " << totalLost << std::endl;

  Simulator::Destroy ();
  return 0;
}