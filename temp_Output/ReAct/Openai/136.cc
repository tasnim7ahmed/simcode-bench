#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

void
TxCallback(Ptr<const Packet> packet)
{
  NS_LOG_INFO("Packet transmitted, size: " << packet->GetSize() << " bytes");
}

void
RxCallback(Ptr<const Packet> packet, const Address &from)
{
  NS_LOG_INFO("Packet received from " << InetSocketAddress::ConvertFrom(from).GetIpv4() 
               << ", size: " << packet->GetSize() << " bytes");
}

int
main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;  // bytes
  uint32_t numPackets = 5;
  double interval = 1.0; // seconds

  CommandLine cmd;
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpEchoSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  UdpEchoServerHelper server(port);
  ApplicationContainer serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  UdpEchoClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(numPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  Ptr<Application> app = clientApp.Get(0);
  Ptr<UdpEchoClient> echoClient = DynamicCast<UdpEchoClient>(app);
  if (echoClient)
  {
    echoClient->TraceConnectWithoutContext("Tx", MakeCallback(&TxCallback));
  }

  app = serverApp.Get(0);
  Ptr<UdpEchoServer> echoServer = DynamicCast<UdpEchoServer>(app);
  if (echoServer)
  {
    echoServer->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}