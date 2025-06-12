#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create nodes: 1 AP, 3 STAs
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (3);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Configure Wi-Fi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Configure Wi-Fi MAC
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  Ssid ssid = Ssid ("ns3-wifi");

  WifiMacHelper mac;

  // Configure STA devices
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Configure AP device
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Set up mobility
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (5.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Install the internet stack
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
  uint16_t port = 9;
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApp = udpServer.Install (wifiApNode.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Install UDP clients on each STA
  uint32_t maxPacketCount = 1000;
  Time interPacketInterval = Seconds (0.01);
  uint32_t packetSize = 1024;

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
      UdpClientHelper udpClient (apInterface.GetAddress (0), port);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
      udpClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
      udpClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

      ApplicationContainer clientApp = udpClient.Install (wifiStaNodes.Get (i));
      clientApp.Start (Seconds (2.0));
      clientApp.Stop (Seconds (10.0));

      clientApps.Add (clientApp);
    }

  // Enable pcap tracing
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-ap", apDevice.Get (0));
  for (uint32_t i = 0; i < staDevices.GetN (); ++i)
    {
      phy.EnablePcap ("wifi-sta" + std::to_string (i), staDevices.Get (i));
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}