/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BasicWifiUdpEchoExample");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (2);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  // Set up Wi-Fi physical and MAC layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;

  // Set SSID
  Ssid ssid = Ssid ("ns3-wifi-ssid");

  // Configure STA
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Configure AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility model for all nodes
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (5.0),
                                "DeltaY", DoubleValue (10.0),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);

  // Internet stack
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

  // UDP Echo Server on STA 1 (second STA)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApp = echoServer.Install (wifiStaNodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (20.0));

  // UDP Echo Client on STA 0 (first STA)
  UdpEchoClientHelper echoClient (staInterfaces.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApp = echoClient.Install (wifiStaNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (20.0));

  // Enable PCAP tracing
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifi-ap", apDevice.Get (0));
  phy.EnablePcap ("wifi-sta1", staDevices.Get (0));
  phy.EnablePcap ("wifi-sta2", staDevices.Get (1));

  // Run simulation
  Simulator::Stop (Seconds (21.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}