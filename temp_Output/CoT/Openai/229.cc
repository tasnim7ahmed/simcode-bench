#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t serverPort = 9;

  UdpServerHelper server(serverPort);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(2.0));

  UdpClientHelper client(interfaces.GetAddress(1), serverPort);
  client.SetAttribute("MaxPackets", UintegerValue(1));
  client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client.SetAttribute("PacketSize", UintegerValue(16));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(2.0));

  Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> packet, const Address &){
    NS_LOG_UNCOND("Server received a packet of size: " << packet->GetSize());
  }));

  Simulator::Stop(Seconds(2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}