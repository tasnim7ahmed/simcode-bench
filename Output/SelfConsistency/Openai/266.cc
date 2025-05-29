#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // 1. Create two nodes
  NodeContainer nodes;
  nodes.Create(2);

  // 2. Configure and install a Point-to-Point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // 3. Install TCP/IP stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // 4. Assign IPv4 addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // 5. Install UDP Echo Server on node 1 (second node)
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);

  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // 6. Install UDP Echo Client on node 0 (first node)
  UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // 7. Run simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}