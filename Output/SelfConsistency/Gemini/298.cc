#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHandoverUdp");

int main(int argc, char *argv[]) {
  // Set up tracing and command line arguments
  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Enable logging
  LogComponentEnable("LteHandoverUdp", LOG_LEVEL_INFO);
  LogComponentEnable("UdpClient", LOG_LEVEL_ALL);
  LogComponentEnable("UdpServer", LOG_LEVEL_ALL);

  // Create LTE Helper
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  // Create eNodeBs
  NodeContainer enbNodes;
  enbNodes.Create(2);

  // Create UE
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Add LTE stack to eNodeBs
  NetDeviceContainer enbDevs;
  enbDevs = lteHelper->InstallEnbDevice(enbNodes);

  // Add LTE stack to UEs
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper->InstallUeDevice(ueNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue("Time"),
                            "Time", StringValue("1s"),
                            "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
  mobility.Install(ueNodes);

  // Set fixed position for eNodeBs
  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
  enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
  enbPositionAlloc->Add(Vector(20.0, 20.0, 0.0));

  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator(enbPositionAlloc);
  enbMobility.Install(enbNodes);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  // Attach UE to eNodeB
  lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

  // Set active bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_MOD_AND_AMBR;
  EpsBearer bearer(q);
  lteHelper->ActivateDedicatedEpsBearer(ueDevs, bearer, EpcTft::Default());

  // Create UDP application (client on UE, server on eNodeB)
  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(enbIpIface.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01))); // packets per second
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

    // Add X2 interface
  lteHelper->AddX2Interface(enbNodes);

  // Enable handover
  lteHelper->HandoverRequest(Seconds(2.0), ueDevs.Get(0), enbDevs.Get(0), enbDevs.Get(1));
  lteHelper->HandoverRequest(Seconds(5.0), ueDevs.Get(0), enbDevs.Get(1), enbDevs.Get(0));


  // Simulation time
  Simulator::Stop(Seconds(10.0));

  // Run simulation
  Simulator::Run();

  // Cleanup
  Simulator::Destroy();

  return 0;
}