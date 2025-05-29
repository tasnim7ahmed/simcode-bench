#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteUePhy::HarqProcessesForDl", UintegerValue (8));
  Config::SetDefault ("ns3::LteUePhy::HarqProcessesForUl", UintegerValue (8));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

  Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
  lteHelper->SetEpcHelper(epcHelper);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NodeContainer epcNodes;
  epcNodes.Add(epcHelper->GetPgwNode());
  NetDeviceContainer epcLinks = p2ph.Install(epcNodes.Get(0), enbNodes.Get(0));

  InternetStackHelper internet;
  internet.Install(epcNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIface = ipv4h.Assign(epcLinks);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  UdpClientHelper client(ueIpIface.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(2));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  client.SetAttribute("PacketSize", UintegerValue(512));
  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(9);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}