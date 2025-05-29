#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUeUdpNetAnim");

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Enable logging from the LteUeUdpNetAnim component
  LogComponentEnable("LteUeUdpNetAnim", LOG_LEVEL_INFO);

  // Create LTE helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Create EPC helper
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // Create a PGW node
  Ptr<Node> pgw = epcHelper->GetPgwNode();

  // Create a remote host node
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);

  // Create Internet Stack
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  // Create channels between PGW and remote host
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
  NetDeviceContainer pgwNetDevice = p2ph.Install(pgw, remoteHost);

  // Assign IP address
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(pgwNetDevice);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Create eNodeB and UE nodes
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Configure Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  mobility.Install(enbNodes);
  mobility.SetPosition(enbNodes.Get(0), Vector(0, 0, 0));

  mobility.Install(ueNodes);
  mobility.SetPosition(ueNodes.Get(0), Vector(10, 10, 0));
  mobility.SetPosition(ueNodes.Get(1), Vector(20, 20, 0));

  // Install LTE devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Attach UEs to the eNodeB
  lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

  // Activate a data radio bearer between UE and eNodeB
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
  EpsBearer bearer(q);
  lteHelper->ActivateDataRadioBearer(ueLteDevs, bearer);

  // Install internet stack on the UEs
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  // Set remote host as the default gateway for the UE
  for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
    Ptr<Node> ue = ueNodes.Get(u);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting(ue)->GetStaticRouting();
    ueStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1); //dummy network
  }

  // Create UDP application
  uint16_t port = 9;  // Discard port (RFC 863)

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
  {
      UdpClientHelper client(remoteHostAddr, port);
      client.SetAttribute("MaxPackets", UintegerValue(1000));
      client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
      client.SetAttribute("PacketSize", UintegerValue(1024));
      clientApps.Add(client.Install(ueNodes.Get(i)));
  }

  clientApps.Start(Seconds(5));
  clientApps.Stop(Seconds(10));

  // Create sink application on the remote host
  PacketSinkHelper sink(port);
  ApplicationContainer sinkApps = sink.Install(remoteHost);
  sinkApps.Start(Seconds(5));
  sinkApps.Stop(Seconds(10));

  // Animation Interface
  AnimationInterface anim("lte-ue-udp-netanim.xml");
  anim.SetConstantPosition(enbNodes.Get(0), 0, 0);
  anim.SetConstantPosition(ueNodes.Get(0), 10, 10);
  anim.SetConstantPosition(ueNodes.Get(1), 20, 20);
  anim.SetConstantPosition(remoteHost, 50, 50);

  // Run simulation
  Simulator::Stop(Seconds(11));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}