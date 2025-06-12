#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  // Create LTE Helper
  LteHelper lteHelper;

  // Create eNodeB and UE nodes
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Create LTE devices and install them on the nodes
  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;

  // Install LTE Devices to the nodes
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomRectanglePositionAllocator",
                            "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                            "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.Install(ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
  enbNodes.Get(0)->AggregateObject(enbMobility);
  Vector enbPosition(50.0, 50.0, 0.0);
  enbMobility->SetPosition(enbPosition);

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  // Assign IP addresses
  Ipv4InterfaceContainer ueIpIface;
  Ipv4InterfaceContainer enbIpIface;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  lteHelper.AssignIpv4Address(NetDeviceContainer(ueDevs), "10.1.1.0", "255.255.255.0");
  ueIpIface = internet.AssignIpv4Interfaces(ueDevs);

  lteHelper.AssignIpv4Address(NetDeviceContainer(enbDevs), "10.1.2.0", "255.255.255.0");
  enbIpIface = internet.AssignIpv4Interfaces(enbDevs);

  // Set up routing: eNB to UE
  ipv4RoutingHelper.SetDefaultRoutes(enbNodes.Get(0)->GetObject<Ipv4>(), "10.1.1.0", "255.255.255.0", "10.1.2.1");

  // Set up routing: UE to eNB
  ipv4RoutingHelper.SetDefaultRoutes(ueNodes.Get(0)->GetObject<Ipv4>(), "10.1.2.0", "255.255.255.0", "10.1.1.1");

  // Install applications
  uint16_t dlPort = 5000;

  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(ueNodes.Get(0));
  dlPacketSinkApp.Start(Seconds(1.0));
  dlPacketSinkApp.Stop(Seconds(10.0));

  UdpClientHelper dlClientHelper(ueIpIface.GetAddress(0), dlPort);
  dlClientHelper.SetAttribute("MaxPackets", UintegerValue(49));
  dlClientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
  dlClientHelper.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer dlClientApp = dlClientHelper.Install(enbNodes.Get(0));
  dlClientApp.Start(Seconds(2.0));
  dlClientApp.Stop(Seconds(10.0));

  // Attach UEs to the closest eNodeB
  lteHelper.Attach(ueDevs, enbDevs.Get(0));

  // Activate a data radio bearer between UE and eNB
  enum EpsBearer::Qci q = EpsBearer::GBR_QCI_DEFAULT;
  EpsBearer bearer(q);
  lteHelper.ActivateDataRadioBearer(ueNodes, bearer, enbDevs.Get(0));

  // Enable PHY tracing
  lteHelper.EnablePhyTraces();

  // Enable MAC tracing
  lteHelper.EnableMacTraces();

  // Enable RLC tracing
  lteHelper.EnableRlcTraces();

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  Simulator::Destroy();

  return 0;
}