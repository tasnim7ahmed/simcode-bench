#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/log.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(1);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbLteDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueLteDevs);

  lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

  uint16_t dlPort = 12345;

  UdpServerHelper dlPacketSinkHelper(dlPort);
  ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(ueNodes.Get(0));
  dlPacketSinkApp.Start(Seconds(1.0));
  dlPacketSinkApp.Stop(Seconds(10.0));

  UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
  dlClient.SetAttribute("MaxPackets", UintegerValue(1000));
  dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
  dlClient.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer dlClientApp = dlClient.Install(enbNodes.Get(0));
  dlClientApp.Start(Seconds(2.0));
  dlClientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();

  Simulator::Destroy();
  return 0;
}