#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpClientServer");

void ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet)
    {
      NS_LOG_INFO ("Received one packet!");
    }
}

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nClients = 3;

  CommandLine cmd;
  cmd.AddValue ("nClients", "Number of client nodes", nClients);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer wifiNodes;
  wifiNodes.Create (nClients + 1); // nClients + server

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiNodes.Get (0));
  for (uint32_t i = 1; i <= nClients; ++i) {
      staDevices.Add (wifi.Install (phy, mac, wifiNodes.Get (i)));
  }

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiNodes.Get (0));

  InternetStackHelper internet;
  internet.Install (wifiNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (NetDeviceContainer (apDevices,staDevices));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (wifiNodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  for (uint32_t i = 1; i <= nClients; ++i) {
      UdpClientHelper client (i.GetAddress (0), port);
      client.SetAttribute ("MaxPackets", UintegerValue (10));
      client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      apps = client.Install (wifiNodes.Get (i));
      apps.Start (Seconds (2.0));
      apps.Stop (Seconds (10.0));
  }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}