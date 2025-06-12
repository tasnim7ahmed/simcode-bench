#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimplexUdpExample");

int main (int argc, char *argv[])
{
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t nSta = 3;
  double simTime = 10.0; // seconds
  uint16_t port = 4000;

  // Create nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Wi-Fi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Wi-Fi MAC and helper configuration
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-80211n");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // AP config
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility - grid for STAs, AP static
  MobilityHelper mobility;

  // STAs in a grid layout
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  // AP at (10,0,0)
  mobility.SetPositionAllocator ("ns3::ListPositionAllocator",
    "PositionList", VectorValue (
      std::vector<Vector> ({Vector (10.0, 0.0, 0.0)})));
  mobility.Install (wifiApNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);

  // Install UDP server on AP
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiApNode.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // Install UDP clients on each STA
  for (uint32_t i = 0; i < nSta; ++i)
    {
      UdpClientHelper client (apInterface.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (5));
      client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      client.SetAttribute ("PacketSize", UintegerValue (512));
      ApplicationContainer clientApp = client.Install (wifiStaNodes.Get (i));
      clientApp.Start (Seconds (1.0));
      clientApp.Stop (Seconds (simTime));
    }

  // Enable pcap tracing (uncomment if needed)
  // phy.EnablePcap ("wifi-simplex-udp", apDevice.Get (0));
  // phy.EnablePcap ("wifi-simplex-udp", staDevices);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}