#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Simulate a link failure after 5 seconds
  Simulator::Schedule(Seconds(5.0), [&]() {
    devices.Get(0)->SetAttribute("DataRate", StringValue("0bps"));
    devices.Get(1)->SetAttribute("DataRate", StringValue("0bps"));
    std::cout << "Link failure at " << Simulator::Now() << std::endl;
  });

  // Restore the link after 7 seconds
   Simulator::Schedule(Seconds(7.0), [&]() {
    devices.Get(0)->SetAttribute("DataRate", StringValue("5Mbps"));
    devices.Get(1)->SetAttribute("DataRate", StringValue("5Mbps"));
    std::cout << "Link restored at " << Simulator::Now() << std::endl;
  });

  Simulator::Stop(Seconds(11.0));

  // Animation
  AnimationInterface anim("distance-vector-loop.xml");
  anim.SetConstantPosition(nodes.Get(0), 10, 10);
  anim.SetConstantPosition(nodes.Get(1), 50, 10);

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}