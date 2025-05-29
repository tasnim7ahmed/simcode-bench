#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/rip.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer routers;
  routers.Create(5);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices[4];
  for (int i = 0; i < 4; ++i) {
    NodeContainer link;
    link.Add(routers.Get(0));
    link.Add(routers.Get(i + 1));
    devices[i] = p2p.Install(link);
  }

  InternetStackHelper internet;
  internet.Install(routers);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer interfaces[4];
  for (int i = 0; i < 4; ++i) {
    std::ostringstream subnet;
    subnet << "10.1." << i + 1 << ".0";
    address.SetBase(subnet.str().c_str(), "255.255.255.0");
    interfaces[i] = address.Assign(devices[i]);
  }

  RipHelper ripRouting;
  GlobalRoutingHelper::PopulateRoutingTables();

  for (uint32_t i = 0; i < routers.GetN(); ++i) {
    Ptr<Node> node = routers.Get(i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
      Ptr<Ipv4Interface> interface = ipv4->GetInterface(j);
      if (interface->GetIpv4Address() != Ipv4Address("127.0.0.1")) {
          ripRouting.AddInterface(i, interface->GetIfIndex());
      }
    }
  }

  TypeId tid = TypeId::LookupByName("ns3::UdpEchoClient");
  Ptr<UdpEchoClientApplication> echoClient = CreateObject<UdpEchoClientApplication>();
  echoClient->SetAttribute("RemoteAddress", AddressValue(interfaces[0].GetAddress(1)));
  echoClient->SetAttribute("RemotePort", UintegerValue(9));
  echoClient->SetAttribute("MaxPackets", UintegerValue(1));
  echoClient->SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient->SetAttribute("PacketSize", UintegerValue(1024));
  routers.Get(1)->AddApplication(echoClient);
  echoClient->SetStartTime(Seconds(10.0));
  echoClient->SetStopTime(Seconds(20.0));

  Simulator::Stop(Seconds(30.0));

  AnimationInterface anim("rip-star.xml");

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}