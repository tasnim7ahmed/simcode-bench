#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <sstream>

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(16383));

  NodeContainer meshNodes;
  meshNodes.Create(4);

  MeshHelper mesh;
  mesh.SetStandard(WIFI_PHY_STANDARD_80211_AC);
  mesh.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode",
                               StringValue("VhtMcs7"), "ControlMode",
                               StringValue("VhtMcs0"));

  NetDeviceContainer meshDevices = mesh.Install(meshNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX",
                                DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(10.0), "DeltaY",
                                DoubleValue(10.0), "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(meshNodes);

  InternetStackHelper internet;
  internet.Install(meshNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(15.0));

  UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(3));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(15.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(15.0));

  std::stringstream pcapFileName;
  pcapFileName << "mesh-example.pcap";
  PointToPointHelper pointToPoint;
  pointToPoint.EnablePcapAll(pcapFileName.str());

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
           stats.begin();
       i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
  }

  Simulator::Destroy();

  return 0;
}