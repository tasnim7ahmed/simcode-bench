#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue (320));

  NodeContainer enbNodes;
  enbNodes.Create(1);
  NodeContainer ueNodes;
  ueNodes.Create(1);

  LteHelper lteHelper;
  lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");
  lteHelper.SetAttribute ("PathlossModel", StringValue("ns3::LogDistancePropagationLossModel"));
  lteHelper.SetAttribute ("FadingModel", StringValue("ns3::TraceFadingLossModel"));
  lteHelper.SetFadingModelAttribute ("TraceFilename", StringValue ("src/lte/model/fading-traces/fading_trace_EPA_3kmph.fad"));

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  Ptr<Node> enbNode = enbNodes.Get(0);
  Ptr<Node> ueNode = ueNodes.Get(0);

  lteHelper.Attach (ueLteDevs, enbLteDevs.Get(0));

  InternetStackHelper internet;
  internet.Install (ueNodes);
  internet.Install (enbNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueLteDevs);
  Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbLteDevs);

  Ipv4Address ueAddr = ueIpIface.GetAddress (0);
  Ipv4Address enbAddr = enbIpIface.GetAddress (0);

  uint16_t port = 9;

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (enbNodes.Get(0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (enbAddr, port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (ueNodes.Get(0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}