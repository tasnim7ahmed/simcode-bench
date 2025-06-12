#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-ospf-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinkStateRoutingSimulation");

class LsaTracer {
public:
  static void LsaUpdateCallback(Ptr<const Packet> packet, const Ipv4Header &ipHeader, uint16_t protocol, Ptr<Ipv4Interface> interface) {
    if (protocol == Ipv4OspfRouting::PROT_NUMBER) {
      NS_LOG_INFO("LSA Update captured on interface " << interface->GetAddress(0).GetLocal());
    }
  }
};

int main(int argc, char *argv[]) {
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("LinkStateRoutingSimulation", LOG_LEVEL_INFO);

  // Create nodes (routers)
  NodeContainer routers;
  routers.Create(4);

  // Create point-to-point links between routers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer link0 = p2p.Install(routers.Get(0), routers.Get(1));
  NetDeviceContainer link1 = p2p.Install(routers.Get(1), routers.Get(2));
  NetDeviceContainer link2 = p2p.Install(routers.Get(2), routers.Get(3));
  NetDeviceContainer link3 = p2p.Install(routers.Get(3), routers.Get(0));

  // Install Internet stack with OSPF routing
  InternetStackHelper internet;
  Ipv4OspfRoutingHelper ospfRouting;

  internet.SetRoutingHelper(ospfRouting); // has to be set before installing
  internet.Install(routers);

  // Assign IP addresses
  Ipv4AddressHelper ip;
  ip.SetBase("10.0.0.0", "255.255.255.0");

  Ipv4InterfaceContainer ifaces0 = ip.Assign(link0);
  ip.NewNetwork();
  Ipv4InterfaceContainer ifaces1 = ip.Assign(link1);
  ip.NewNetwork();
  Ipv4InterfaceContainer ifaces2 = ip.Assign(link2);
  ip.NewNetwork();
  Ipv4InterfaceContainer ifaces3 = ip.Assign(link3);

  // Enable pcap tracing for LSA analysis
  p2p.EnablePcapAll("lsa-trace");

  // Connect LSA update trace
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::Ipv4L3Protocol/InterfaceList/*/$ns3::Ipv4Interface/Tx", MakeCallback(&LsaTracer::LsaUpdateCallback));

  // Create a host node connected to router 0
  NodeContainer host;
  host.Create(1);
  NetDeviceContainer hostLink = p2p.Install(host.Get(0), routers.Get(0));
  ip.NewNetwork();
  Ipv4InterfaceContainer hostIfaces = ip.Assign(hostLink);
  internet.Install(host);

  // Set up traffic from host to router 2
  uint16_t port = 9; // Discard port
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(ifaces1.GetAddress(1), port));
  onoff.SetConstantRate(DataRate("1kbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer apps = onoff.Install(host);
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  // Sink to receive the packets
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(routers.Get(2));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}