#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  LogComponent::SetLevel( "UdpClient", LOG_LEVEL_INFO );
  LogComponent::SetLevel( "UdpServer", LOG_LEVEL_INFO );

  Config::SetDefault ("ns3::LteUePhy::HarqProcessesPerUe", UintegerValue (8));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(3);

  // Configure LTE network
  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18100));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

  Ptr<Node> pgw = CreateObject<Node>();

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(pgw);
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  NetDeviceContainer pgwEnbIface = p2p.Install(pgw, enbNodes.Get(0));

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIface = ipv4h.Assign(pgwEnbIface);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
  pgwStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), internetIface.GetAddress(1), 1);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("7.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

  // Set mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomRectanglePositionAllocator",
                            "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                            "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.Install(ueNodes);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
  enbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
  mobility.SetPositionAllocator(enbPositionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);

  // Install applications
  uint16_t port = 5000;

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(ueIpIface.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Enable PCAP tracing
  p2p.EnablePcapAll("lte-udp");
  lteHelper.EnableTraces();

  // Run simulation
  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}