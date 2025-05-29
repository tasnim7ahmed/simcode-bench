#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleUdpExample");

int main (int argc, char *argv[])
{
  // Disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // Turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));

  NodeContainer wifiNodes;
  wifiNodes.Create (2); // Node 0: AP, Node 1: STA

  // Set up WiFi channel and physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Set up WiFi
  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ssid");

  // Configure STA device
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiNodes.Get (1));

  // Configure AP device
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiNodes.Get (0));

  // Mobility: Place nodes for good connectivity
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP
  positionAlloc->Add (Vector (1.0, 0.0, 0.0)); // STA close
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  // Internet stack & assign IPs
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevice);

  // UDP server on AP (port 9), runs from 1s to 10s
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiNodes.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // UDP client on STA, sends packets to AP
  UdpClientHelper client (apInterfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (10000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (wifiNodes.Get (1));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // Enable PCAP Tracing (optional, comment out to disable)
  //phy.EnablePcap ("wifi-simple-udp-ap", apDevice.Get(0));
  //phy.EnablePcap ("wifi-simple-udp-sta", staDevice.Get(0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}