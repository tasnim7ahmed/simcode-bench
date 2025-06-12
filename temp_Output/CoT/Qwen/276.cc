#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpApStaSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes: one AP and three STAs
  NodeContainer apNode;
  apNode.Create (1);

  NodeContainer staNodes;
  staNodes.Create (3);

  // Create Wi-Fi channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  // Configure MAC and PHY
  WifiMacHelper mac;
  WifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  // Set remote station manager
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs7"), "ControlMode", StringValue ("HtMcs0"));

  // Create AP and STA devices
  mac.SetType ("ns3::ApWifiMac");
  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("wifi-network")));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  // Place nodes in grid layout
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);
  mobility.Install (staNodes);

  // Install internet stack
  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  // Setup UDP server on AP
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // Setup UDP clients on each STA sending to the AP
  uint32_t packetSize = 512;
  Time interPacketInterval = Seconds (1.0);
  uint32_t maxPacketCount = 5;

  for (uint32_t i = 0; i < staNodes.GetN (); ++i)
    {
      UdpEchoClientHelper echoClient (apInterface.GetAddress (0), 9);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
      echoClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
      echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer clientApps = echoClient.Install (staNodes.Get (i));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (10.0));
    }

  // Enable PCAP tracing
  phy.EnablePcapAll ("wifi_udp_ap_sta_simulation");

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}