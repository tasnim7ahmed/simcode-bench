#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdHocSimulation");

class AodvHelper : public aodv::AodvHelper
{
public:
  AodvHelper() : aodv::AodvHelper()
  {
    Set("EnableHello", BooleanValue(true));
    Set("HelloInterval", TimeValue(Seconds(1.0)));
    Set("RreqRetries", UintegerValue(2));
  }
};

int main(int argc, char *argv[])
{
  bool enableRoutingTablePrint = false;
  double routingPrintInterval = 10.0;
  double simulationTime = 50.0;
  uint32_t numNodes = 10;
  std::string pcapFileName = "aodv_simulation.pcap";
  std::string flowMonitorFile = "aodv_simulation.flowmon";

  CommandLine cmd(__FILE__);
  cmd.AddValue("enableRoutingTablePrint", "Enable periodic printing of routing tables", enableRoutingTablePrint);
  cmd.AddValue("routingPrintInterval", "Interval (seconds) between routing table prints", routingPrintInterval);
  cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  PointToPointStarHelper star(numNodes, PointToPointHelper());
  star.Install(nodes);

  // Mobility setup
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(20.0),
                                "DeltaY", DoubleValue(20.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  mobility.Install(nodes);

  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(star.GetSpokesDevices());

  // UDP Echo Server and Client Setup
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    serverApps.Add(echoServer.Install(nodes.Get(i)));
  }
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  for (uint32_t i = 0; i < numNodes; ++i)
  {
    uint32_t j = (i + 1) % numNodes; // Next node in circular fashion
    UdpEchoClientHelper echoClient(interfaces.GetAddress(j), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(4.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime - 1));
  }

  // Enable PCAP tracing
  for (uint32_t i = 0; i < numNodes; ++i)
  {
    star.GetHubDevice().EnablePcap(pcapFileName + "-" + std::to_string(i), star.GetHubDevice(), true, true);
    star.GetSpokeDevices(i).EnablePcap(pcapFileName + "-" + std::to_string(i), star.GetSpokeDevices(i), true, true);
  }

  // Routing table printing
  if (enableRoutingTablePrint)
  {
    Ipv4StaticRoutingHelper routingHelper;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<Node> node = nodes.Get(i);
      Simulator::Schedule(Seconds(routingPrintInterval), &Ipv4StaticRoutingHelper::PrintRoutingTableAllAt,
                          &routingHelper, Seconds(routingPrintInterval), node);
    }
  }

  // Flow monitor setup
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Animation output
  NetAnimXmlWriter anim("aodv_simulation.xml");
  anim.Write(nodes, Seconds(0.1));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  monitor->SerializeToXmlFile(flowMonitorFile, true, true);

  Simulator::Destroy();
  return 0;
}