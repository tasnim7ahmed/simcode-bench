#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  // Point-to-point link with low bandwidth and low queue size to induce loss
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
  // DropTail queue with max 5 packets
  pointToPoint.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(5));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // UDP server on node 1 (receiver)
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // UDP client on node 0 (sender) -- send at much higher rate than link, so loss occurs
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 1000;
  Time interPacketInterval = Seconds(0.001); // 1ms, i.e. 1000 packets/sec (overload)
  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
  client.SetAttribute("Interval", TimeValue(interPacketInterval));
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll("packet-loss-demo", true);

  // Set up NetAnim
  AnimationInterface anim("packet-loss-demo.xml");
  anim.SetConstantPosition(nodes.Get(0), 50, 50);
  anim.SetConstantPosition(nodes.Get(1), 250, 50);

  // Visualize packet drops at Queue
  Ptr<NetDevice> dev = devices.Get(0);
  Ptr<PointToPointNetDevice> ptpDev = dev->GetObject<PointToPointNetDevice>();
  Ptr<Queue<Packet> > queue = ptpDev->GetQueue();

  queue->TraceConnectWithoutContext("Drop", MakeCallback([](Ptr<const Packet> packet){
    NS_LOG_UNCOND("Packet dropped at queue at " << Simulator::Now().GetSeconds() << " s");
  }));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}