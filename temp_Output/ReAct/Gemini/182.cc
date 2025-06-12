#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (160));

  NodeContainer enbNodes;
  enbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  // Create LTE Helper
  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (5000));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18000));

  // EPC Helper
  Ptr<LteEPC> epcHelper = CreateObject<LteEPC>();
  lteHelper.SetEpcHelper(epcHelper);

  // Create a remote host
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);

  // Create the internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
  p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
  NetDeviceContainer internetDevices = p2ph.Install(remoteHost, epcHelper->GetSpGwP());
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase("1.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  // Install Mobility Model
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(enbNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

  // Attach the UEs to the closest eNodeB
  lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

  // Set active UEs
  lteHelper.ActivateDedicatedBearer(ueLteDevs, EpsBearer(EpsBearer::Qci::GBR_CONV_VOICE));

  // Install the internet stack on the UEs
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIface = ipv4h.Assign(ueLteDevs);

  // Assign IP address to UEs
  ipv4h.SetBase("7.0.0.0", "255.0.0.0");
  Ipv4StaticRoutingHelper ipv4RoutingHelperUe;

  Ptr<Ipv4StaticRouting> ueStaticRouting;
  ueStaticRouting = ipv4RoutingHelperUe.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
  ueStaticRouting->SetDefaultRoute(epcHelper->GetSpGwP()->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1);
  ueStaticRouting = ipv4RoutingHelperUe.GetStaticRouting(ueNodes.Get(1)->GetObject<Ipv4>());
  ueStaticRouting->SetDefaultRoute(epcHelper->GetSpGwP()->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1);

  // Create Applications
  uint16_t port = 5000;
  UdpClientHelper client(ueIpIface.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(ueNodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Enable Tracing
  // p2ph.EnablePcapAll("lte-tcp");

  // NetAnim
  AnimationInterface anim("lte-netanim.xml");
  anim.SetConstantPosition(enbNodes.Get(0), 10.0, 10.0);
  anim.SetConstantPosition(ueNodes.Get(0), 20.0, 20.0);
  anim.SetConstantPosition(ueNodes.Get(1), 30.0, 30.0);
  anim.SetConstantPosition(remoteHost, 40.0, 40.0);

  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}