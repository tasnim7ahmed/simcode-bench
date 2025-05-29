#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("5gNrUeGnbExample");

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetLevel( "UdpClient", LOG_LEVEL_INFO );
  LogComponent::SetLevel( "UdpServer", LOG_LEVEL_INFO );

  // Create Nodes: gNB and UE
  NodeContainer gnbNodes;
  gnbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // EPC Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  // Create a remote host
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  // PointToPoint link between the remote host and the EPC
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
  NetDeviceContainer ইন্টারনেট devices;
  devices = p2ph.Install(remoteHostContainer, epcHelper->GetPgwNode());

  Ipv4AddressHelper ip;
  ip.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ip.Assign(devices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(gnbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(gnbNodes);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
  mobility.Install(ueNodes);

  // Install internet stack on the UE
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
  // Assign IP address to UEs, and install applications
  Ptr<Node> ue = ueNodes.Get(0);
  Ipv4Address ueAddr = ueIpIface.GetAddress(0);

  // Attach one UE to the first eNodeB
  lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

  // Set active connections
  ue->GetObject<LteUeNetDevice>()->GetRrc()->SetConnectionState(Rrc::RRC_CONNECTED);

  // Install and start applications on UEs and remote host
  uint16_t dlPort = 1234;
  UdpClientHelper dlClient(remoteHostAddr, dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
  dlClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer dlClientApps = dlClient.Install(ueNodes.Get(0));
  dlClientApps.Start(Seconds(1));
  dlClientApps.Stop(Seconds(10));

  UdpServerHelper dlPacketSink(dlPort);
  ApplicationContainer dlServerApps = dlPacketSink.Install(remoteHost);
  dlServerApps.Start(Seconds(1));
  dlServerApps.Stop(Seconds(10));

  // Run the simulation
  Simulator::Stop(Seconds(10));
  Simulator::Run();

  // Cleanup
  Simulator::Destroy();
  return 0;
}