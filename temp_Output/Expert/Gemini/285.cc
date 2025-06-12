#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000000));
  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (512));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer enbNodes;
  enbNodes.Create (1);

  NodeContainer ueNodes;
  ueNodes.Create (3);

  LteHelper lteHelper;
  lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (100));
  lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18100));

  Point2D enbPosition(50.0, 50.0);
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);
  Ptr<ConstantPositionMobilityModel> enbMobMod = enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  enbMobMod->SetPosition(enbPosition);

  MobilityHelper ueMobility;
  ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Mode", StringValue ("Time"),
                               "Time", StringValue ("1s"),
                               "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  ueMobility.Install (ueNodes);

  NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice (ueNodes);

  lteHelper.Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));
  lteHelper.Attach (ueLteDevs.Get (1), enbLteDevs.Get (0));
  lteHelper.Attach (ueLteDevs.Get (2), enbLteDevs.Get (0));

  InternetStackHelper internet;
  internet.Install (enbNodes);
  internet.Install (ueNodes);

  Ipv4InterfaceContainer ueIpIface;
  Ipv4InterfaceContainer enbIpIface;
  Ipv4AddressHelper ipAddrHelper;
  ipAddrHelper.SetBase ("10.1.1.0", "255.255.255.0");
  ueIpIface = ipAddrHelper.Assign (ueLteDevs);
  enbIpIface = ipAddrHelper.Assign (enbLteDevs);

  UdpServerHelper server (5000);
  ApplicationContainer serverApps = server.Install (ueNodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (ueIpIface.GetAddress (0), 5000);
  ApplicationContainer clientApps = client.Install (ueNodes.Get (1));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowMonitorHelper;
  flowMonitor = flowMonitorHelper.InstallAll();

  Simulator::Stop (Seconds (10));

  lteHelper.EnablePcapAll ("lte-simulation");

  Simulator::Run ();

  flowMonitor->SerializeToXmlFile("lte-simulation.flowmon", false, true);

  Simulator::Destroy ();

  return 0;
}