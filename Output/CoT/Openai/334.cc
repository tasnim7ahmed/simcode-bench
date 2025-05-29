#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer wifiStaNode;
  wifiStaNode.Create (1);
  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  // STA: Random mobility
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Bounds", RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)));
  mobility.Install (wifiStaNode);

  // AP: Fixed position at (50,50)
  MobilityHelper apMobility;
  apMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  apMobility.Install (wifiApNode);
  Ptr<MobilityModel> apMob = wifiApNode.Get (0)->GetObject<MobilityModel> ();
  apMob->SetPosition (Vector (50.0, 50.0, 0.0));

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces, apInterfaces;
  staInterfaces = address.Assign (staDevice);
  apInterfaces = address.Assign (apDevice);

  uint16_t port = 9;

  // UDP Server (AP)
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiApNode.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (20.0));

  // UDP Client (STA)
  UdpClientHelper client (apInterfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (wifiStaNode.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (15.0));

  phy.EnablePcap ("wifi-simple", apDevice.Get (0));
  phy.EnablePcap ("wifi-simple-sta", staDevice.Get (0));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}