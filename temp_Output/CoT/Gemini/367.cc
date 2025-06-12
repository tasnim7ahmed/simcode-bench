#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbComponentCarrier::DlEarfcn", UintegerValue (7));
  Config::SetDefault ("ns3::LteEnbComponentCarrier::UlEarfcn", UintegerValue (30007));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (7));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (30007));

  NetDeviceContainer enbDevs;
  enbDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueDevs;
  ueDevs = lteHelper.InstallUeDevice(ueNodes);

  lteHelper.Attach(ueDevs.Get(0), enbDevs.Get(0));

  InternetStackHelper internet;
  internet.Install(enbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign(enbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  UintegerValue port = 5000;

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (enbIpIface.GetAddress (0), port.Get ()));
  ApplicationContainer sinkApp = sinkHelper.Install (enbNodes.Get(0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper onOffHelper ("ns3::TcpSocketFactory", InetSocketAddress (enbIpIface.GetAddress (0), port.Get ()));
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mb/s")));
  ApplicationContainer clientApp = onOffHelper.Install (ueNodes.Get(0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}