#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdp");

int main(int argc, char *argv[]) {

  // Enable logging from the LteUdp component
  LogComponentEnable("LteUdp", LOG_LEVEL_INFO);

  // Set up command line arguments
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Create EPC helper
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // Create a remote host
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  // Create the internet link between the remote host and the EPC
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2ph.SetChannelAttribute("Delay", StringValue("1ms"));
  NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetSpGateway(), remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
  // set the default gateway for the remote host
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  // Create eNodeB node
  NodeContainer enbNodes;
  enbNodes.Create(1);

  // Create UE nodes
  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Install the IP stack on the UEs
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  // Set the default gateway for the UE
  for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
  {
    Ptr<Node> ueNode = ueNodes.Get(u);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
  }

  // Attach one UE per eNodeB
  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));
  lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0));

  // Set the positions of the nodes
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // eNodeB
  positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // UE1
  positionAlloc->Add(Vector(20.0, 0.0, 0.0)); // UE2
  MobilityHelper mobility;
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Create UDP application:
  //  - UE1 sends two 512-byte packets to UE2 on port 9
  uint16_t port = 9;
  Address remoteAddress(InetSocketAddress(ueIpIface.GetAddress(1), port));

  UdpClientHelper clientHelper(remoteAddress);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(2));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(5.0)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper serverHelper(port);
  ApplicationContainer serverApps = serverHelper.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  // Enable PCAP tracing
  // p2ph.EnablePcapAll("lte-udp");
  // lteHelper->EnableTraces();

  // Animation Interface
  // Ptr<AnimationInterface> anim = CreateObject<AnimationInterface> ("lte-udp.anim");
  // anim->SetMaxPktsPerTraceFile(1000000);

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Cleanup
  Simulator::Destroy();

  return 0;
}