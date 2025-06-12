#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
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

  uint16_t serverPort = 4000;

  UdpServerHelper udpServer(serverPort);
  ApplicationContainer serverApps = udpServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(2.0));

  UdpClientHelper udpClient(interfaces.GetAddress(1), serverPort);
  udpClient.SetAttribute("MaxPackets", UintegerValue(1));
  udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  udpClient.SetAttribute("PacketSize", UintegerValue(32));

  ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(0.5));
  clientApps.Stop(Seconds(2.0));

  Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", 
    MakeCallback([](Ptr<const Packet> packet, const Address &addr) {
      NS_LOG_UNCOND("Server received one packet at " << Simulator::Now().GetSeconds() << "s");
    }));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}