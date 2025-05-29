#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRandomWalkExample");

int
main (int argc, char *argv[])
{
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Parameters
  uint32_t nStas = 2;
  double simulationTime = 20.0; // seconds
  double interval = 1.0; // seconds between UDP packets
  uint16_t port = 9; // Well-known echo port
  uint32_t packetSize = 1024; // bytes

  // Create nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nStas);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Set up WiFi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Set up WiFi MAC and standard
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility for STAs: RandomWalk2d in 100m x 100m area
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  mobility.Install (wifiStaNodes);

  // Mobility for AP: Fixed
  MobilityHelper apMobility;
  apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  apMobility.SetPositionAllocator ("ns3::ListPositionAllocator",
                                   "PositionList", VectorValue (std::vector<Vector> { Vector (50.0, 50.0, 0.0) }));
  apMobility.Install (wifiApNode);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // Install UDP server on AP
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApp = udpServer.Install (wifiApNode.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime + 1));

  // Install UDP client on each STA targeting the AP
  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nStas; ++i)
    {
      UdpClientHelper udpClient (apInterface.GetAddress (0), port);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t (simulationTime / interval)));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));
      clientApps.Add (udpClient.Install (wifiStaNodes.Get (i)));
    }

  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  Simulator::Stop (Seconds (simulationTime + 2));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}