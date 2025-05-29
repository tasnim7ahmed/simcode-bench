#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiHttpSim");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  // Set up WiFi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (Ssid ("ns3-wifi")),
                   "ActiveProbing", BooleanValue (false));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  NetDeviceContainer staDevices, apDevices;
  staDevices = wifi.Install (phy, wifiMac, wifiNodes.Get (0));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (Ssid ("ns3-wifi")));
  apDevices = wifi.Install (phy, wifiMac, wifiNodes.Get (1));

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (5.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (staDevices, apDevices));

  // Install a "HTTP" server
  uint16_t httpPort = 80;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), httpPort));

  // Use PacketSink to simulate HTTP server receiving traffic
  PacketSinkHelper httpServerHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), httpPort));
  ApplicationContainer serverApp = httpServerHelper.Install (wifiNodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Use BulkSendApplication to simulate HTTP client GET request
  BulkSendHelper httpClientHelper ("ns3::TcpSocketFactory", serverAddress);
  httpClientHelper.SetAttribute ("MaxBytes", UintegerValue (512)); // simulate a small web page
  ApplicationContainer clientApp = httpClientHelper.Install (wifiNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Enable pcap tracing for Wi-Fi
  phy.EnablePcap ("wifi-http", apDevices.Get (0), true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}