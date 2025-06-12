#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaUdpIpvXExample");

int
main (int argc, char *argv[])
{
  bool useIpv6 = false;
  bool enableLogs = false;

  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Set to true to use IPv6 addressing, false for IPv4", useIpv6);
  cmd.AddValue ("enableLogs", "Set to true to enable logging", enableLogs);
  cmd.Parse (argc, argv);

  if (enableLogs)
    {
      LogComponentEnable ("CsmaUdpIpvXExample", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    }

  Ptr<Node> n0 = CreateObject<Node> ();
  Ptr<Node> n1 = CreateObject<Node> ();
  NodeContainer nodes (n0, n1);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4Address serverIpv4;
  Ipv6Address serverIpv6;
  uint16_t port = 4000;

  if (!useIpv6)
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
      serverIpv4 = interfaces.GetAddress (1);
      NS_LOG_INFO ("Server IPv4 address: " << serverIpv4);
    }
  else
    {
      Ipv6AddressHelper ipv6;
      ipv6.SetBase ("2001:1::", 64);
      Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);
      interfaces.SetForwarding (0, true);
      interfaces.SetDefaultRouteInAllNodes (0);
      serverIpv6 = interfaces.GetAddress (1,1);
      NS_LOG_INFO ("Server IPv6 address: " << serverIpv6);
    }

  // Create UDP server on n1
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (n1);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // Create UDP client on n0
  Address clientAddr;
  if (!useIpv6)
    {
      clientAddr = InetSocketAddress (serverIpv4, port);
    }
  else
    {
      clientAddr = Inet6SocketAddress (serverIpv6, port);
    }

  uint32_t maxPackets = 320;
  Time interPacketInterval = MilliSeconds (50);
  uint32_t packetSize = 1024;

  UdpClientHelper client (clientAddr);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = client.Install (n0);
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  NS_LOG_INFO ("Starting simulation...");
  Simulator::Run ();
  NS_LOG_INFO ("Simulation finished.");
  Simulator::Destroy ();
  return 0;
}