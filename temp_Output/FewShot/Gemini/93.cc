#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/command-line.h"
#include "ns3/socket.h"
#include "ns3/ipv4-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpLanSimulation");

static void
ReceivePacket(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);

  Ipv4Header ipHeader;
  packet->PeekHeader(ipHeader);

  NS_LOG_INFO("Received one packet from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                          << " with TOS: " << (uint16_t)ipHeader.GetTos()
                                          << " and TTL: " << (uint16_t)ipHeader.GetTtl());
}


static void
InstallReceiver(NodeContainer& nodes, uint16_t port, bool recvTos, bool recvTtl)
{
  Ptr<Node> receiver = nodes.Get(1);
  Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4>();
  Ipv4Address interfaceAddress = ipv4->GetAddress(1, 0).GetLocal();

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> socket = Socket::CreateSocket(receiver, tid);
  InetSocketAddress local = InetSocketAddress(interfaceAddress, port);
  socket->Bind(local);
  socket->SetRecvCallback(MakeCallback(&ReceivePacket));

    if (recvTos) {
        socket->SetAttribute("IpRecvTos", BooleanValue(true));
    }

    if (recvTtl) {
        socket->SetAttribute("IpRecvTtl", BooleanValue(true));
    }

  socket->SetAttribute ("ReceiveBufferSize", UintegerValue(65535));
}

int
main(int argc, char* argv[])
{
  bool verbose = false;
  uint32_t nPackets = 5;
  Time interPacketInterval = Seconds(1.0);
  uint32_t packetSize = 1024;
  uint16_t port = 9;
  uint8_t tos = 0;
  uint8_t ttl = 64;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("nPackets", "Number of packets generated", nPackets);
  cmd.AddValue("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue("interval", "Interval between packets", interPacketInterval);
  cmd.AddValue("port", "Port number to send UDP packets to", port);
  cmd.AddValue("tos", "Type of Service (TOS) value for packets", tos);
  cmd.AddValue("ttl", "Time To Live (TTL) value for packets", ttl);
    cmd.AddValue("recvTos", "Enable receiving TOS value", recvTos);
    cmd.AddValue("recvTtl", "Enable receiving TTL value", recvTtl);

  cmd.Parse(argc, argv);

  if (verbose)
  {
    LogComponentEnable("UdpLanSimulation", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  InstallReceiver(nodes, port, recvTos, recvTtl);

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(nPackets));
  client.SetAttribute("Interval", TimeValue(interPacketInterval));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  client.SetAttribute("IpTos", UintegerValue(tos));
  client.SetAttribute("IpTtl", UintegerValue(ttl));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}