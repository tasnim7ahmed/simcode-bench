#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveNodeMulticastExample");

class Stats {
public:
  uint32_t rxPackets = 0;
  uint32_t txPackets = 0;
  void RxCallback(Ptr<const Packet> p, const Address &address) { rxPackets++; }
  void TxCallback(Ptr<const Packet> p) { txPackets++; }
};

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create nodes: 0=A, 1=B, 2=C, 3=D, 4=E
  NodeContainer nodes;
  nodes.Create(5);

  // Links: (A-B), (A-C), (C-D), (D-E), (B-E)
  // We'll blacklist (A-B) and (D-E), i.e., don't use them for multicast
  // Build all links for flexibility: keep all, but setup routes to blacklist specific paths.

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // A-B
  NetDeviceContainer d_ab = p2p.Install(nodes.Get(0), nodes.Get(1));
  // A-C
  NetDeviceContainer d_ac = p2p.Install(nodes.Get(0), nodes.Get(2));
  // C-D
  NetDeviceContainer d_cd = p2p.Install(nodes.Get(2), nodes.Get(3));
  // D-E
  NetDeviceContainer d_de = p2p.Install(nodes.Get(3), nodes.Get(4));
  // B-E
  NetDeviceContainer d_be = p2p.Install(nodes.Get(1), nodes.Get(4));

  // Internet stack (with StaticRouting)
  InternetStackHelper stack;
  Ipv4ListRoutingHelper list;
  Ipv4StaticRoutingHelper staticRouting;
  Ipv4GlobalRoutingHelper globalRouting;
  list.Add(staticRouting, 0);
  list.Add(globalRouting, 10);
  stack.SetRoutingHelper(list);
  stack.Install(nodes);

  // Assign IPs
  Ipv4AddressHelper ipv4;
  Ipv4InterfaceContainer i_ab, i_ac, i_cd, i_de, i_be;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  i_ab = ipv4.Assign(d_ab);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  i_ac = ipv4.Assign(d_ac);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  i_cd = ipv4.Assign(d_cd);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  i_de = ipv4.Assign(d_de);

  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  i_be = ipv4.Assign(d_be);

  // Multicast group: 225.1.2.4
  Ipv4Address multicastAddress("225.1.2.4");
  Ipv4StaticRoutingHelper multicast;
  Ptr<Node> sender = nodes.Get(0);

  // Source interface (A)
  Ptr<NetDevice> senderIf = d_ac.Get(0); // Use A-C for multicast out
  uint32_t senderIfIndex = sender->GetObject<Ipv4>()->GetInterfaceForDevice(senderIf);

  // Add multicast route on A out to C only
  Ptr<Ipv4StaticRouting> Astatic = multicast.GetStaticRouting(sender->GetObject<Ipv4>());
  Ipv4Address networkA("225.1.2.4");
  Ipv4Mask maskA("255.255.255.255");
  std::vector<uint32_t> outputInterfacesA;
  outputInterfacesA.push_back(senderIfIndex); // Interface A-C
  Astatic->AddMulticastRoute(multicastAddress, Ipv4Address("0.0.0.0"), outputInterfacesA);

  // Subscribe C, D, E to group
  nodes.Get(2)->GetObject<Ipv4>()->AddMulticastAddress(senderIfIndex,
    multicastAddress); // on C's A-C interface
  nodes.Get(2)->GetObject<Ipv4>()->AddMulticastAddress(
    nodes.Get(2)->GetObject<Ipv4>()->GetInterfaceForDevice(d_cd.Get(0)), multicastAddress);

  nodes.Get(3)->GetObject<Ipv4>()->AddMulticastAddress(
    nodes.Get(3)->GetObject<Ipv4>()->GetInterfaceForDevice(d_cd.Get(1)), multicastAddress);
  nodes.Get(3)->GetObject<Ipv4>()->AddMulticastAddress(
    nodes.Get(3)->GetObject<Ipv4>()->GetInterfaceForDevice(d_de.Get(0)), multicastAddress);

  nodes.Get(4)->GetObject<Ipv4>()->AddMulticastAddress(
    nodes.Get(4)->GetObject<Ipv4>()->GetInterfaceForDevice(d_de.Get(1)), multicastAddress);
  nodes.Get(4)->GetObject<Ipv4>()->AddMulticastAddress(
    nodes.Get(4)->GetObject<Ipv4>()->GetInterfaceForDevice(d_be.Get(1)), multicastAddress);

  // Multicast routes
  // A: (out A-C only)
  // C: in A-C, out C-D
  Ptr<Ipv4StaticRouting> Cstatic = multicast.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
  std::vector<uint32_t> cin;
  cin.push_back(nodes.Get(2)->GetObject<Ipv4>()->GetInterfaceForDevice(d_cd.Get(0)));
  Cstatic->AddMulticastRoute(multicastAddress,
    i_ac.GetAddress(1), // from A-C interface
    cin
  );

  // D: in C-D, out D-E
  Ptr<Ipv4StaticRouting> Dstatic = multicast.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
  std::vector<uint32_t> din;
  din.push_back(nodes.Get(3)->GetObject<Ipv4>()->GetInterfaceForDevice(d_de.Get(0)));
  Dstatic->AddMulticastRoute(multicastAddress, i_cd.GetAddress(1), din);

  // B: in B-E (simulate that multicast can be received via B-E only)
  Ptr<Ipv4StaticRouting> Bstatic = multicast.GetStaticRouting(nodes.Get(1)->GetObject<Ipv4>());
  std::vector<uint32_t> bin;
  Bstatic->AddMulticastRoute(multicastAddress,
    i_be.GetAddress(0), bin // for B, only input interface matters
  );

  // E: in D-E and in B-E (can receive via both links)
  Ptr<Ipv4StaticRouting> Estatic = multicast.GetStaticRouting(nodes.Get(4)->GetObject<Ipv4>());
  std::vector<uint32_t> ein;
  Estatic->AddMulticastRoute(multicastAddress,
    i_de.GetAddress(1), ein);
  Estatic->AddMulticastRoute(multicastAddress,
    i_be.GetAddress(1), ein);

  // Application setup
  // Packet size and rate
  uint32_t packetSize = 256;
  std::string dataRate = "500Kbps";
  double simTime = 5.0;

  // Packet sinks on B, C, D, E
  Stats statsB, statsC, statsD, statsE;
  uint16_t port = 4000;

  // Helper function to install PacketSink on node
  auto installSink = [&](Ptr<Node> n, Stats& s) {
    InetSocketAddress local(multicastAddress, port);
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", local);
    ApplicationContainer sinkApp = sinkHelper.Install(n);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simTime + 1));
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&Stats::RxCallback, &s));
    return sinkApp;
  };

  ApplicationContainer sinkB = installSink(nodes.Get(1), statsB);
  ApplicationContainer sinkC = installSink(nodes.Get(2), statsC);
  ApplicationContainer sinkD = installSink(nodes.Get(3), statsD);
  ApplicationContainer sinkE = installSink(nodes.Get(4), statsE);

  // OnOff UDP sender on A
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(multicastAddress, port));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
  ApplicationContainer srcApp = onoff.Install(nodes.Get(0));

  // Connect Tx trace on the onoff application's socket
  Ptr<Application> app = srcApp.Get(0);
  Ptr<OnOffApplication> onoffApp = DynamicCast<OnOffApplication>(app);
  Ptr<Socket> sock = onoffApp->GetSocket();
  Stats statsTx;
  if (sock) {
    sock->TraceConnectWithoutContext("Tx", MakeCallback(&Stats::TxCallback, &statsTx));
  }

  // Enable pcap tracing
  p2p.EnablePcapAll("five-node-multicast");

  Simulator::Stop(Seconds(simTime + 1));
  Simulator::Run();

  std::cout << "Total Packets Sent:    " << statsTx.txPackets << std::endl;
  std::cout << "Total Packets Received:\n";
  std::cout << "    Node B: " << statsB.rxPackets << std::endl;
  std::cout << "    Node C: " << statsC.rxPackets << std::endl;
  std::cout << "    Node D: " << statsD.rxPackets << std::endl;
  std::cout << "    Node E: " << statsE.rxPackets << std::endl;
  std::cout << "    Sum   : " << statsB.rxPackets + statsC.rxPackets +
                                   statsD.rxPackets + statsE.rxPackets << std::endl;

  Simulator::Destroy();
  return 0;
}