#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", StringValue ("sf10"));
  Config::SetDefault ("ns3::LteEnbRrc::DlEarfcn", UintegerValue (38200));
  Config::SetDefault ("ns3::LteUeRrc::DlEarfcn", UintegerValue (38200));
  Config::SetDefault ("ns3::LteEnbRrc::UlEarfcn", UintegerValue (38200 + 18000));
  Config::SetDefault ("ns3::LteUeRrc::UlEarfcn", UintegerValue (38200 + 18000));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(gnbNodes);

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-10.0, -10.0, 10.0, 10.0)));
  mobility.Install(ueNodes);

  // Devices
  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(38200));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(38200 + 18000));
  NetDeviceContainer enbDevs = lteHelper.InstallEnbDevice(gnbNodes);
  NetDeviceContainer ueDevs = lteHelper.InstallUeDevice(ueNodes);

  // Attach UEs
  lteHelper.Attach(ueDevs.Get(0), enbDevs.Get(0));
  lteHelper.Attach(ueDevs.Get(1), enbDevs.Get(0));

  // Activate default EPS bearer
  enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearer(q);
  lteHelper.ActivateDedicatedEpsBearer(ueDevs, bearer, EpcTft::Default());

  // Internet stack
  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);

  // Applications
  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(ueIpIface.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(5));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}