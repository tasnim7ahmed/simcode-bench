#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create Nodes
  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  // Create LTE Helper
  LteHelper lteHelper;
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper.SetEpcHelper(epcHelper);

  // Create and install the transport network nodes
  NetDeviceContainer enbNetDev = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueNetDev = lteHelper.InstallUeDevice(ueNodes);

  // Install the IP stack on the UEs
  InternetStackHelper internet;
  ueNodes.Add(enbNodes.Get(0));
  internet.Install(ueNodes);

  // Assign IP address to UEs, and add a default routing to the epc
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueNetDev);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbNetDev);

  // Attach one UE per eNB
  lteHelper.Attach(ueNetDev.Get(0), enbNetDev.Get(0));

  // Set up the UDP Echo Server on eNB
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApp = echoServer.Install(enbNodes.Get(0));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // Set up the UDP Echo Client on UE
  UdpEchoClientHelper echoClient(enbIpIface.GetAddress(0), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = echoClient.Install(ueNodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Mobility model
  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
  enbNodes.Get(0)->AggregateObject(enbMobility);
  enbMobility->SetPosition(Vector(0, 0, 0));

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  ueMobility.Install(ueNodes);
  Ptr<ConstantPositionMobilityModel> uemob = CreateObject<ConstantPositionMobilityModel>();
  ueNodes.Get(0)->AggregateObject(uemob);
  uemob->SetPosition(Vector(20, 20, 0));

  // Run simulation
  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}