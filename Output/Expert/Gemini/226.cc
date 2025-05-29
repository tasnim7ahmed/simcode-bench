#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  uint32_t numNodes = 10;
  double simulationTime = 50.0;

  NodeContainer sensorNodes;
  sensorNodes.Create(numNodes);

  NodeContainer sinkNode;
  sinkNode.Create(1);

  NodeContainer allNodes;
  allNodes.Add(sensorNodes);
  allNodes.Add(sinkNode);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"), "RtsCtsThreshold", UintegerValue(2200));

  NetDeviceContainer sensorDevices = wifi.Install(phy, sensorNodes);
  NetDeviceContainer sinkDevice = wifi.Install(phy, sinkNode);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper(olsr);
  internet.Install(allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign(sensorDevices);
  Ipv4InterfaceContainer sinkInterface = ipv4.Assign(sinkDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(sinkNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(simulationTime));

  UdpEchoClientHelper echoClient(sinkInterface.GetAddress(0), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
  echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes; ++i) {
    clientApps.Add(echoClient.Install(sensorNodes.Get(i)));
  }

  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(simulationTime));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                "X", StringValue("0.0"),
                                "Y", StringValue("0.0"),
                                "Z", StringValue("0.0"),
                                "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(allNodes);

  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set("PeriodicEnergyUpdateInterval", TimeValue(Seconds(1)));
  EnergySourceContainer sources = basicSourceHelper.Install(allNodes);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set("TxCurrentA", DoubleValue(0.0174));
  radioEnergyModelHelper.Set("RxCurrentA", DoubleValue(0.0197));
  radioEnergyModelHelper.Set("SleepCurrentA", DoubleValue(0.0000011));
  radioEnergyModelHelper.Set("SwitchingCurrentA", DoubleValue(0.0000088));

  DeviceEnergyModelContainer deviceModels = radioEnergyModelHelper.Install(sensorDevices);
  DeviceEnergyModelContainer sinkDeviceModels = radioEnergyModelHelper.Install(sinkDevice);

  for (uint32_t i = 0; i < allNodes.GetN(); ++i) {
      Ptr<Node> node = allNodes.Get(i);
      Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(sources.Get(i));
      Ptr<DeviceEnergyModel> deviceModel;
      if (i < numNodes) {
          deviceModel = DynamicCast<DeviceEnergyModel>(deviceModels.Get(i));
      } else {
          deviceModel = DynamicCast<DeviceEnergyModel>(sinkDeviceModels.Get(0));
      }
      source->SetNode(node);
      if(deviceModel){
          deviceModel->SetEnergySource(source);
      }
  }

  AnimationInterface anim("olsr-energy.xml");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));

  Simulator::Run();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";

    }

  Simulator::Destroy();

  return 0;
}