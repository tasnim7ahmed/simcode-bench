#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceStaticRouting");

void
ReceivePacket(Ptr<Socket> socket, std::string nodeName)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
      NS_LOG_UNCOND("[" << Simulator::Now().GetSeconds() << "s] "
                        << nodeName << " received "
                        << packet->GetSize()
                        << " bytes from "
                        << address.GetIpv4());
    }
}

void
SendPacket(Ptr<Socket> srcSocket, Ipv4Address dstAddr, uint16_t dstPort, uint32_t size)
{
  Ptr<Packet> packet = Create<Packet>(size);
  srcSocket->SendTo(packet, 0, InetSocketAddress(dstAddr, dstPort));
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("MultiInterfaceStaticRouting", LOG_LEVEL_INFO);

  // Create nodes: Source, Router1, Router2, Destination
  NodeContainer nodes;
  nodes.Create(4);
  Ptr<Node> src = nodes.Get(0);
  Ptr<Node> r1  = nodes.Get(1);
  Ptr<Node> r2  = nodes.Get(2);
  Ptr<Node> dst = nodes.Get(3);

  // PointToPoint helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // ---- Establish 2 parallel paths: src--r1--dst and src--r2--dst ----
  // Path 1: src <-> r1 <-> dst
  NodeContainer n_src_r1(src, r1);
  NodeContainer n_r1_dst(r1, dst);

  // Path 2: src <-> r2 <-> dst
  NodeContainer n_src_r2(src, r2);
  NodeContainer n_r2_dst(r2, dst);

  // NetDevices
  NetDeviceContainer d_src_r1 = p2p.Install(n_src_r1);
  NetDeviceContainer d_r1_dst = p2p.Install(n_r1_dst);
  NetDeviceContainer d_src_r2 = p2p.Install(n_src_r2);
  NetDeviceContainer d_r2_dst = p2p.Install(n_r2_dst);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_src_r1 = ipv4.Assign(d_src_r1);

  ipv4.SetBase("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r1_dst = ipv4.Assign(d_r1_dst);

  ipv4.SetBase("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_src_r2 = ipv4.Assign(d_src_r2);

  ipv4.SetBase("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r2_dst = ipv4.Assign(d_r2_dst);

  // Each node's interface summary:
  // src: 10.0.1.1 (if #1), 10.0.3.1 (if #2)
  // r1:  10.0.1.2 (if #1), 10.0.2.1 (if #2)
  // r2:  10.0.3.2 (if #1), 10.0.4.1 (if #2)
  // dst: 10.0.2.2 (if #1), 10.0.4.2 (if #2)

  // Static Routing: Manually set routes with different metrics
  Ipv4StaticRoutingHelper staticRoutingHelper;

  // ----- On source -----
  Ptr<Ipv4> ipv4Src = src->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticSrc = staticRoutingHelper.GetStaticRouting(ipv4Src);

  // Route via r1 (Path 1): Next hop 10.0.1.2 via interface 0, to dst (10.0.2.2)
  staticSrc->AddNetworkRouteTo(
      Ipv4Address("10.0.2.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.1.2"), 1, 1); // IfIndex=1 (10.0.1.1), metric=1

  // Route via r2 (Path 2): Next hop 10.0.3.2 via interface 2, to dst (10.0.4.2)
  staticSrc->AddNetworkRouteTo(
      Ipv4Address("10.0.4.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.3.2"), 2, 5); // IfIndex=2 (10.0.3.1), higher metric=5

  // Default route for other cases through r1
  staticSrc->SetDefaultRoute(Ipv4Address("10.0.1.2"), 1, 1);

  // ----- On r1 -----
  Ptr<Ipv4> ipv4R1 = r1->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticR1 = staticRoutingHelper.GetStaticRouting(ipv4R1);
  // Forward to dst via 10.0.2.2
  staticR1->AddNetworkRouteTo(
      Ipv4Address("10.0.2.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.2.2"), 2, 1);
  // Forward to src via 10.0.1.1
  staticR1->AddNetworkRouteTo(
      Ipv4Address("10.0.1.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.1.1"), 1, 1);
  // Forward to r2/dst via src
  staticR1->AddNetworkRouteTo(
      Ipv4Address("10.0.4.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.1.1"), 1, 5);

  // ----- On r2 -----
  Ptr<Ipv4> ipv4R2 = r2->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticR2 = staticRoutingHelper.GetStaticRouting(ipv4R2);
  staticR2->AddNetworkRouteTo(
      Ipv4Address("10.0.4.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.4.2"), 2, 1);
  staticR2->AddNetworkRouteTo(
      Ipv4Address("10.0.3.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.3.1"), 1, 1);
  staticR2->AddNetworkRouteTo(
      Ipv4Address("10.0.2.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.3.1"), 1, 5);

  // ----- On dst -----
  Ptr<Ipv4> ipv4Dst = dst->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticDst = staticRoutingHelper.GetStaticRouting(ipv4Dst);
  staticDst->AddNetworkRouteTo(
      Ipv4Address("10.0.1.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.2.1"), 1, 1);
  staticDst->AddNetworkRouteTo(
      Ipv4Address("10.0.3.0"), Ipv4Mask("/24"),
      Ipv4Address("10.0.4.1"), 2, 1);

  // UDP port to use
  uint16_t dstPort = 9000;

  // ----------- Sockets for source on both interfaces ------------
  // Source has two interfaces: 10.0.1.1 (ifIndex 1), 10.0.3.1 (ifIndex 2)
  // We'll create two sockets, one bound to each address

  // Interface 1 socket
  Ptr<Socket> srcSocket1 = Socket::CreateSocket(src, UdpSocketFactory::GetTypeId());
  InetSocketAddress localAddr1 = InetSocketAddress(Ipv4Address("10.0.1.1"), 0);
  srcSocket1->Bind(localAddr1);
  srcSocket1->SetRecvCallback(MakeBoundCallback(&ReceivePacket, "Source/if-1"));

  // Interface 2 socket
  Ptr<Socket> srcSocket2 = Socket::CreateSocket(src, UdpSocketFactory::GetTypeId());
  InetSocketAddress localAddr2 = InetSocketAddress(Ipv4Address("10.0.3.1"), 0);
  srcSocket2->Bind(localAddr2);
  srcSocket2->SetRecvCallback(MakeBoundCallback(&ReceivePacket, "Source/if-2"));

  // ---------- Socket for destination -----------
  Ptr<Socket> dstSocket = Socket::CreateSocket(dst, UdpSocketFactory::GetTypeId());
  InetSocketAddress dstBindAddr = InetSocketAddress(Ipv4Address::GetAny(), dstPort);
  dstSocket->Bind(dstBindAddr);
  dstSocket->SetRecvCallback(MakeBoundCallback(&ReceivePacket, "Destination"));

  // --------------- Simulate UDP sending via both interfaces ---------------
  // Send a packet via interface 1 at 1.0s
  Simulator::Schedule(
      Seconds(1.0),
      &SendPacket, srcSocket1, Ipv4Address("10.0.2.2"), dstPort, 512);

  // Send a packet via interface 2 at 2.0s (should prefer route via r1, but we use lower metric for if-1)
  Simulator::Schedule(
      Seconds(2.0),
      &SendPacket, srcSocket2, Ipv4Address("10.0.4.2"), dstPort, 512);

  // Send a burst of packets alternating interfaces
  for (uint32_t i = 0; i < 4; ++i)
    {
      if (i % 2 == 0)
        {
          Simulator::Schedule(
              Seconds(3.0 + i * 0.5),
              &SendPacket, srcSocket1, Ipv4Address("10.0.2.2"), dstPort, 512);
        }
      else
        {
          Simulator::Schedule(
              Seconds(3.0 + i * 0.5),
              &SendPacket, srcSocket2, Ipv4Address("10.0.4.2"), dstPort, 512);
        }
    }

  // Run simulation
  Simulator::Stop(Seconds(6.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}