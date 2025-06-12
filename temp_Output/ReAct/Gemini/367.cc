#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  NetDeviceContainer enbDevs;
  NetDeviceContainer ueDevs;

  lteHelper->SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (500));
  lteHelper->SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18500));

  enbDevs = lteHelper->InstallEnbDevice(enbNodes);
  ueDevs = lteHelper->InstallUeDevice(ueNodes);

  InternetStackHelper internet;
  internet.Install(ueNodes);
  internet.Install(enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  Ptr<Node> ue = ueNodes.Get(0);
  Ptr<Node> enb = enbNodes.Get(0);

  lteHelper->Attach(ueDevs, enbDevs.Get(0));

  uint16_t dlPort = 1000;
  uint16_t ulPort = 2000;

  Address enbAddress(InetSocketAddress(enbIpIface.GetAddress(0), dlPort));
  BulkSendHelper client("ns3::TcpSocketFactory", enbAddress);
  client.SetAttribute("SendSize", UintegerValue(1024));
  client.SetAttribute("MaxBytes", UintegerValue(10000000));
  ApplicationContainer clientApps = client.Install(ue);
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(9.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), dlPort));
  ApplicationContainer sinkApps = sink.Install(enb);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}