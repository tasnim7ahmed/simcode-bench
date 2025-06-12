#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::EnableUeMeasurements", BooleanValue (false));

  NodeContainer ueNodes;
  ueNodes.Create(1);
  NodeContainer gnbNodes;
  gnbNodes.Create(1);
  NodeContainer enbNodes;
  enbNodes.Create(0);

  NodeContainer cnNodes;
  cnNodes.Create(1);

  PointToPointHelper p2pCn;
  p2pCn.SetDeviceAttribute("DataRate", StringValue("100Gb/s"));
  p2pCn.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer cnDevs = p2pCn.Install(gnbNodes.Get(0), cnNodes.Get(0));

  InternetStackHelper internet;
  internet.Install(cnNodes);
  internet.Install(ueNodes);

  NetDeviceContainer ueLteDevs;

  Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
  nrHelper->SetSchedulerType (NrPointToPointEpsBearer::SCHED_TYPE_PF);

  NetDeviceContainer enbLteDevs = nrHelper->InstallGnb(gnbNodes);
  ueLteDevs = nrHelper->InstallUeDevice(ueNodes);

  internet.Install(gnbNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer internetif = ipv4h.Assign(cnDevs);

  Ipv4InterfaceContainer ueIpIface = nrHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  Ipv4InterfaceContainer gnbIpIface = nrHelper->AssignGnbIpv4Address(NetDeviceContainer(enbLteDevs));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t dlPort = 9;
  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(ueNodes.Get(0));
  dlPacketSinkApp.Start(Seconds(1.0));
  dlPacketSinkApp.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  Time interPacketInterval = MilliSeconds(10);
  UdpClientHelper dlClientHelper(ueIpIface.GetAddress(0), dlPort);
  dlClientHelper.SetAttribute("MaxPackets", UintegerValue(numPackets));
  dlClientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
  dlClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer dlClientApps = dlClientHelper.Install(gnbNodes.Get(0));
  dlClientApps.Start(Seconds(2.0));
  dlClientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}