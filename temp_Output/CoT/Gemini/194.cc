#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nWifi = 5;
  double simulationTime = 10;

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer apNodes;
  apNodes.Create (2);
  NodeContainer staNodes;
  staNodes.Create (nWifi);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns-3-ssid-1");
  Ssid ssid2 = Ssid ("ns-3-ssid-2");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevs1 = wifi.Install (phy, mac, staNodes.Get (0), staNodes.Get (1));
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevs2 = wifi.Install (phy, mac, staNodes.Get (2), staNodes.Get (3), staNodes.Get (4));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1),
               "BeaconInterval", TimeValue (Seconds (0.01)),
               "QosSupported", BooleanValue (true));
  NetDeviceContainer apDevs1 = wifi.Install (phy, mac, apNodes.Get (0));

   mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2),
               "BeaconInterval", TimeValue (Seconds (0.01)),
               "QosSupported", BooleanValue (true));
  NetDeviceContainer apDevs2 = wifi.Install (phy, mac, apNodes.Get (1));

  NetDeviceContainer devices;
  devices.Add (staDevs1);
  devices.Add (staDevs2);
  devices.Add (apDevs1);
  devices.Add (apDevs2);

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevs1);
  Ipv4InterfaceContainer staInterfaces1 = address.Assign (staDevs1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevs2);
  Ipv4InterfaceContainer staInterfaces2 = address.Assign (staDevs2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpServerHelper server1 (port);
  ApplicationContainer serverApps1 = server1.Install (apNodes.Get (0));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (simulationTime + 1));

  UdpServerHelper server2 (port);
  ApplicationContainer serverApps2 = server2.Install (apNodes.Get (1));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (simulationTime + 1));

  UdpClientHelper client1 (apInterfaces1.GetAddress (0), port);
  client1.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client1.SetAttribute ("Interval", TimeValue (MicroSeconds (1)));
  client1.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps1_1 = client1.Install (staNodes.Get (0));
  clientApps1_1.Start (Seconds (2.0));
  clientApps1_1.Stop (Seconds (simulationTime));
  ApplicationContainer clientApps1_2 = client1.Install (staNodes.Get (1));
  clientApps1_2.Start (Seconds (2.0));
  clientApps1_2.Stop (Seconds (simulationTime));

  UdpClientHelper client2 (apInterfaces2.GetAddress (0), port);
  client2.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client2.SetAttribute ("Interval", TimeValue (MicroSeconds (1)));
  client2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2_1 = client2.Install (staNodes.Get (2));
  clientApps2_1.Start (Seconds (2.0));
  clientApps2_1.Stop (Seconds (simulationTime));
  ApplicationContainer clientApps2_2 = client2.Install (staNodes.Get (3));
  clientApps2_2.Start (Seconds (2.0));
  clientApps2_2.Stop (Seconds (simulationTime));
  ApplicationContainer clientApps2_3 = client2.Install (staNodes.Get (4));
  clientApps2_3.Start (Seconds (2.0));
  clientApps2_3.Stop (Seconds (simulationTime));

  phy.EnablePcap ("wifi-example", devices);

  AsciiTraceHelper ascii;
  phy.EnableAsciiAll (ascii.CreateFileStream ("wifi-example.tr"));

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();

  uint64_t totalBytesSentSta0 = DynamicCast<UdpClient> (clientApps1_1.Get (0)->GetObject<Application>())->m_totalBytes;
  uint64_t totalBytesSentSta1 = DynamicCast<UdpClient> (clientApps1_2.Get (0)->GetObject<Application>())->m_totalBytes;
  uint64_t totalBytesSentSta2 = DynamicCast<UdpClient> (clientApps2_1.Get (0)->GetObject<Application>())->m_totalBytes;
  uint64_t totalBytesSentSta3 = DynamicCast<UdpClient> (clientApps2_2.Get (0)->GetObject<Application>())->m_totalBytes;
  uint64_t totalBytesSentSta4 = DynamicCast<UdpClient> (clientApps2_3.Get (0)->GetObject<Application>())->m_totalBytes;

  std::cout << "Total Bytes Sent by STA 0: " << totalBytesSentSta0 << std::endl;
  std::cout << "Total Bytes Sent by STA 1: " << totalBytesSentSta1 << std::endl;
  std::cout << "Total Bytes Sent by STA 2: " << totalBytesSentSta2 << std::endl;
  std::cout << "Total Bytes Sent by STA 3: " << totalBytesSentSta3 << std::endl;
  std::cout << "Total Bytes Sent by STA 4: " << totalBytesSentSta4 << std::endl;

  Simulator::Destroy ();
  return 0;
}