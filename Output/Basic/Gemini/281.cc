#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/random-walk-2d-mobility-model.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(3);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
  lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18100));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
  lteHelper.SetEpcHelper(epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode();
  NodeContainer remoteNodes;
  remoteNodes.Add(pgw);
  remoteNodes.Create(1);

  InternetStackHelper internet;
  internet.Install(remoteNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueNodes);
  ipv4h.Assign(remoteNodes);

  Ipv4Address remoteAddress = remoteNodes.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  // Install UDP application
  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(remoteNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(remoteAddress, port);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
    clientApps.Add(client.Install(ueNodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Install and configure mobility model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Mode", EnumValue(RandomWalk2dMobilityModel::MODE_TIME),
                            "Time", StringValue("2s"),
                            "Speed", StringValue("1.0m/s"),
                            "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
  mobility.Install(ueNodes);

  Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
  enbNodes.Get(0)->AggregateObject(enbMobility);
  Vector enbPosition(25, 25, 0);
  enbMobility->SetPosition(enbPosition);

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}