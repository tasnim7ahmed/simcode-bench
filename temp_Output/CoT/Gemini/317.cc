#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  Config::SetDefault ("ns3::LteUePhy::EnableUplinkPowerControl", BooleanValue (false));

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");
  lteHelper.SetSchedulerType ("ns3::RrFfMacScheduler");
  lteHelper.SetHandoverAlgorithmType ("ns3::NoHandoverAlgorithm");

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  Ptr<Node> enbNode = enbNodes.Get(0);
  Ptr<Node> ueNode = ueNodes.Get(0);

  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4h.Assign(enbLteDevs);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t dlPort = 9;

  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(enbNodes);
  dlPacketSinkApp.Start(Seconds(1.0));
  dlPacketSinkApp.Stop(Seconds(10.0));

  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  Time interPacketInterval = Seconds(0.01);

  UdpClientHelper dlClientHelper(enbIpIface.GetAddress(0), dlPort);
  dlClientHelper.SetAttribute("MaxPackets", UintegerValue(numPackets));
  dlClientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
  dlClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

  ApplicationContainer dlClientApps = dlClientHelper.Install(ueNodes);
  dlClientApps.Start(Seconds(2.0));
  dlClientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  Simulator::Destroy();

  return 0;
}