#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetOlsrSimulation");

void
PrintRoutingTable(Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
  std::ostringstream oss;
  ipv4->GetRoutingProtocol()->PrintRoutingTable(oss);
  std::cout << "Time " << Simulator::Now().GetSeconds()
            << "s: Node " << node->GetId() << " Routing Table:\n"
            << oss.str() << std::endl;
  Simulator::Schedule(printTime, &PrintRoutingTable, node, printTime);
}

int
main(int argc, char *argv[])
{
  uint32_t numNodes = 6;
  double simTime = 10.0;
  double areaX = 100.0;
  double areaY = 100.0;

  // 1. Create Nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // 2. Wifi Configuration
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // 3. Mobility Configuration
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=5.0|Max=15.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                            "PositionAllocator", PointerValue(
                              CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.Install(nodes);

  // 4. Internet stack with OLSR
  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper(olsr);
  internet.Install(nodes);

  // 5. IP addressing
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // 6. UDP client/server configuration
  uint16_t port = 4000;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(5));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simTime));

  UdpClientHelper client(interfaces.GetAddress(5), port);
  client.SetAttribute("MaxPackets", UintegerValue(1));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simTime));

  // 7. Enable routing table logging
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    Simulator::Schedule(Seconds(0.5), &PrintRoutingTable, nodes.Get(i), Seconds(1.0));
  }

  // 8. Enable logging
  wifiPhy.EnablePcapAll("manet-olsr");

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}