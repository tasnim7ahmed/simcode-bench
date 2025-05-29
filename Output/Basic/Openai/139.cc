#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SixNodeUdpEchoExample");

void
TxTrace (Ptr<const Packet> packet)
{
  NS_LOG_INFO ("Packet transmitted at " << Simulator::Now ().GetSeconds () << "s");
}

void
RxTrace (Ptr<const Packet> packet, const Address &)
{
  NS_LOG_INFO ("Packet received at " << Simulator::Now ().GetSeconds () << "s");
}

int
main(int argc, char *argv[])
{
  LogComponentEnable ("SixNodeUdpEchoExample", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (6);

  // Set up point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
  NetDeviceContainer d34 = p2p.Install(nodes.Get(3), nodes.Get(4));
  NetDeviceContainer d45 = p2p.Install(nodes.Get(4), nodes.Get(5));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign(d01);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign(d12);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign(d23);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i34 = ipv4.Assign(d34);

  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i45 = ipv4.Assign(d45);

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP Echo Server on node 5
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP Echo Client on node 0, sends to node 5
  UdpEchoClientHelper echoClient (i45.GetAddress(1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Tracing packet transmissions and receptions
  Ptr<Application> clientApp = clientApps.Get(0);
  Ptr<UdpEchoClient> udpEchoClient = DynamicCast<UdpEchoClient>(clientApp);
  if (udpEchoClient)
  {
    udpEchoClient->TraceConnectWithoutContext("Tx", MakeCallback(&TxTrace));
  }

  Ptr<Application> serverApp = serverApps.Get(0);
  Ptr<UdpEchoServer> udpEchoServer = DynamicCast<UdpEchoServer>(serverApp);
  if (udpEchoServer)
  {
    udpEchoServer->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));
  }

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}