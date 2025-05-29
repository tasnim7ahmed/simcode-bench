#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetAodvSim");

int main (int argc, char *argv[])
{
  uint32_t numNodes = 4;
  double simTime = 30.0;
  double distance = 100.0;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"), "ControlMode", StringValue ("DsssRate11Mbps"));
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
  mobility.Install (nodes);

  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 4000;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (3), port));
  onoff.SetAttribute ("DataRate", StringValue ("1024kbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (512));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime-1)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (3));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  wifiPhy.EnablePcapAll ("manet-aodv");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  double rxPackets = 0, txPackets = 0, rxBytes = 0, txBytes = 0;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      if (t.destinationAddress == interfaces.GetAddress (3) && t.sourceAddress == interfaces.GetAddress (0))
        {
          txPackets += iter->second.txPackets;
          rxPackets += iter->second.rxPackets;
          txBytes += iter->second.txBytes;
          rxBytes += iter->second.rxBytes;
        }
    }
  double throughput = (rxBytes * 8.0) / (simTime - 1.0) / 1000; // in kbps
  std::cout << "TxPackets: " << txPackets << std::endl;
  std::cout << "RxPackets: " << rxPackets << std::endl;
  std::cout << "PacketLoss: " << txPackets - rxPackets << std::endl;
  std::cout << "Throughput: " << throughput << " kbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}