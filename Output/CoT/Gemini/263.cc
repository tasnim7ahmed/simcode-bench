#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install(ueNodes);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(500));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18500));
  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(ueIpIface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = echoClient.Install(enbNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}