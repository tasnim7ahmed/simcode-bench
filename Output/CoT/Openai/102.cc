#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastStaticRoutingExample");

class PacketReceptionStats
{
public:
  void RxCallback(Ptr<const Packet> packet, const Address &address)
  {
    ++m_received;
    m_bytes += packet->GetSize();
  }
  uint32_t m_received = 0;
  uint32_t m_bytes = 0;
};

int main(int argc, char *argv[])
{
  // Simulation parameters
  double simTime = 10.0;
  std::string dataRate = "2Mbps";
  std::string delay = "2ms";
  uint32_t packetSize = 512;
  std::string appDataRate = "1Mbps";

  // Create nodes
  NodeContainer nodes;
  nodes.Create(5); // A:0, B:1, C:2, D:3, E:4

  // Connect nodes with point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  // Topology: A-B, A-C, C-D, C-E
  NetDeviceContainer devAB = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // A-B
  NetDeviceContainer devAC = pointToPoint.Install(nodes.Get(0), nodes.Get(2)); // A-C
  NetDeviceContainer devCD = pointToPoint.Install(nodes.Get(2), nodes.Get(3)); // C-D
  NetDeviceContainer devCE = pointToPoint.Install(nodes.Get(2), nodes.Get(4)); // C-E

  // Install Internet stack
  InternetStackHelper internet;
  Ipv4StaticRoutingHelper staticRouting;
  internet.SetRoutingHelper(staticRouting);
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifAB = address.Assign(devAB);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifAC = address.Assign(devAC);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifCD = address.Assign(devCD);

  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ifCE = address.Assign(devCE);

  // Define multicast group
  Ipv4Address multicastSource("10.1.1.1"); // Node A's interface on A-B
  Ipv4Address multicastGroup("225.1.2.4");

  // Static multicast routing: configure group membership
  Ptr<Node> nodeA = nodes.Get(0);
  Ptr<Node> nodeB = nodes.Get(1);
  Ptr<Node> nodeC = nodes.Get(2);
  Ptr<Node> nodeD = nodes.Get(3);
  Ptr<Node> nodeE = nodes.Get(4);

  // Configure static multicast routing on each node
  // Node A: send out both interfaces except A-B or A-C as blacklists (if required)
  Ptr<Ipv4StaticRouting> staticRoutingA = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(nodeA->GetObject<Ipv4>());
  staticRoutingA->AddMulticastRoute(1, multicastGroup, ifAC.GetAddress(0), {1}); // "1" is the interface index for A-C

  // Node C: forward multicast to D and E
  Ptr<Ipv4StaticRouting> staticRoutingC = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(nodeC->GetObject<Ipv4>());
  std::vector<uint32_t> outIfC = {2,3}; // ifCD, ifCE indices (2,3), checked below
  for (uint32_t i = 0; i < nodeC->GetObject<Ipv4>()->GetNInterfaces(); ++i)
  {
    Ipv4Address addr = nodeC->GetObject<Ipv4>()->GetAddress(i,0).GetLocal();
    if (addr == ifCD.GetAddress(0) || addr == ifCE.GetAddress(0))
      staticRoutingC->AddMulticastRoute(i, multicastGroup, addr, {i});
  }

  // Node B, D, E: join multicast group
  Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4D = nodeD->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4E = nodeE->GetObject<Ipv4>();
  ipv4B->AddMulticastAddress(1, multicastGroup); // interface 1 (from A-B)
  ipv4D->AddMulticastAddress(1, multicastGroup); // interface 1 (from C-D)
  ipv4E->AddMulticastAddress(1, multicastGroup); // interface 1 (from C-E)

  // Application: OnOff traffic generator on node A
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, 9999));
  onoff.SetConstantRate(DataRate(appDataRate), packetSize);

  ApplicationContainer onoffApps = onoff.Install(nodeA);
  onoffApps.Start(Seconds(1.0));
  onoffApps.Stop(Seconds(simTime - 1));

  // Packet sinks on B,C,D,E
  ApplicationContainer sinks;
  std::vector<std::shared_ptr<PacketReceptionStats>> statsVec(4);
  for (uint32_t n = 1; n <= 4; ++n)
  {
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, 9999));
    ApplicationContainer app = sinkHelper.Install(nodes.Get(n));
    app.Start(Seconds(0.5));
    app.Stop(Seconds(simTime));
    sinks.Add(app);

    Ptr<Application> pktSink = app.Get(0);
    auto stats = std::make_shared<PacketReceptionStats>();
    pktSink->TraceConnectWithoutContext("Rx", MakeCallback(&PacketReceptionStats::RxCallback, stats.get()));
    statsVec[n-1] = stats;
  }

  // Enable pcap tracing on all devices
  pointToPoint.EnablePcapAll("mcast-static-routing");

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  uint32_t totalRx = 0;
  uint32_t totalBytes = 0;
  for (uint32_t i = 0; i < statsVec.size(); ++i)
  {
    std::cout << "Node " << (char)('B'+i) << " received " << statsVec[i]->m_received << " packets ("
              << statsVec[i]->m_bytes << " bytes)" << std::endl;
    totalRx += statsVec[i]->m_received;
    totalBytes += statsVec[i]->m_bytes;
  }
  std::cout << "Total packets received at sink nodes: " << totalRx << " (" << totalBytes << " bytes)" << std::endl;

  Simulator::Destroy();
  return 0;
}