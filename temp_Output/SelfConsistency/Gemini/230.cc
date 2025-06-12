#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpClientServer");

int main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Allow command-line arguments to change simulation parameters (if needed)
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes: a server and multiple clients
  NodeContainer serverNode;
  serverNode.Create (1);

  NodeContainer clientNodes;
  clientNodes.Create (2); // Two client nodes

  // Create a single access point (AP) node
  NodeContainer apNode;
  apNode.Create (1);

  // Combine all nodes into a single container
  NodeContainer allNodes = NodeContainer (serverNode, clientNodes, apNode);

  // Create the Wi-Fi PHY and MAC layers
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Configure the MAC layer for the AP (infrastructure)
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  // Install the AP device on the AP node
  NetDeviceContainer apDevice = wifi.Install (phy, mac, apNode);

  // Configure the MAC layer for the clients (station)
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  // Install the STA devices on the client nodes
  NetDeviceContainer clientDevices = wifi.Install (phy, mac, clientNodes);

  // Install a STA device on the server node to communicate via wifi.
  NetDeviceContainer serverDevice = wifi.Install (phy, mac, serverNode);

  // Install the Internet stack on all nodes
  InternetStackHelper internet;
  internet.Install (allNodes);

  // Assign IP addresses to the devices
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer clientInterface = address.Assign (clientDevices);
  Ipv4InterfaceContainer serverInterface = address.Assign (serverDevice);

  Ipv4Address serverAddr = serverInterface.GetAddress (0);

  // Create a UDP server application on the server node
  UdpServerHelper server (9);
  ApplicationContainer serverApp = server.Install (serverNode.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // Create UDP client applications on the client nodes
  UdpClientHelper client (serverAddr, 9);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < clientNodes.GetN (); ++i)
    {
      clientApps.Add (client.Install (clientNodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (9.0));

  // Enable global static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Run the simulation
  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}