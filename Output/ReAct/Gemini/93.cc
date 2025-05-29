#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include <iostream>

using namespace ns3;

uint32_t receivedPackets = 0;

void ReceivePacket(Ptr<Socket> socket) {
  Address from;
  Ptr<Packet> packet = socket->RecvFrom(from);

  Ipv4Header ipHeader;
  packet->PeekHeader(ipHeader);

  std::cout << Simulator::Now().AsDouble() << "s: Received one packet with ToS " << (uint16_t)ipHeader.GetTos()
            << " and TTL " << (uint16_t)ipHeader.GetTtl() << std::endl;
  receivedPackets++;
}

int main(int argc, char *argv[]) {
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0;
  uint8_t tosValue = 0xa0;
  uint8_t ttlValue = 64;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets sent", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("tos", "IP ToS value", tosValue);
  cmd.AddValue("ttl", "IP TTL value", ttlValue);
  cmd.AddValue("recvTos", "Enable ToS receive option", recvTos);
  cmd.AddValue("recvTtl", "Enable TTL receive option", recvTtl);
  cmd.Parse(argc, argv);

  Time interPacketInterval = Seconds(interval);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
  echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
  echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(1), TypeId::LookupByName("ns3::UdpSocketFactory"));
  InetSocketAddress local = InetSocketAddress(interfaces.GetAddress(1), 9);
  recvSocket->Bind(local);
  recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

  if (recvTos) {
    recvSocket->SetAttribute("IpRecvTos", BooleanValue(true));
  }

  if (recvTtl) {
    recvSocket->SetAttribute("IpRecvTtl", BooleanValue(true));
  }

  Ptr<Socket> sendSocket = Socket::CreateSocket(nodes.Get(0), TypeId::LookupByName("ns3::UdpSocketFactory"));
  sendSocket->SetAttribute("IpTos", UintegerValue(tosValue));
  sendSocket->SetAttribute("IpTtl", UintegerValue(ttlValue));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}