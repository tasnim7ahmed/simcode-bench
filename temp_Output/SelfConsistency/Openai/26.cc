#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/matrix-propagation-loss-model.h"
#include "ns3/ssid.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-phy-standard.h"
#include "ns3/config.h"

using namespace ns3;

void PrintFlowStats(Ptr<FlowMonitor> flowMonitor, FlowMonitorHelper &flowHelper)
{
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
  std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i;
  for (i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = flowHelper.GetClassifier()->FindFlow(i->first);
      std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets:   " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets:   " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      if (i->second.rxPackets > 0)
        {
          double throughput = i->second.rxBytes * 8.0 / 10.0 / 1e6; // 10 second sim, Mbps
          std::cout << "  Throughput:   " << throughput << " Mbps\n";
        }
      else
        {
          std::cout << "  Throughput:   0 Mbps\n";
        }
      std::cout << "  Packet Loss Ratio: "
                << ((i->second.txPackets - i->second.rxPackets) * 100.0 / i->second.txPackets) << " %\n";
    }
}

void RunHiddenTerminal(bool enableRtsCts)
{
  std::cout << "---------------\n";
  std::cout << "RTS/CTS " << (enableRtsCts ? "ENABLED" : "DISABLED") << "\n";
  std::cout << "---------------\n";
  // 1. Create Nodes
  NodeContainer nodes;
  nodes.Create(3);

  // 2. Mobility: place in line so 0-1 and 1-2 are adjacent, but 0-2 are far apart
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
  pos->Add(Vector(0.0, 0.0, 0.0));   // Node 0
  pos->Add(Vector(5.0, 0.0, 0.0));   // Node 1 (center)
  pos->Add(Vector(10.0, 0.0, 0.0));  // Node 2
  mobility.SetPositionAllocator(pos);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  // 3. Wi-Fi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  Ptr<YansWifiChannel> chan = channel.Create();
  // Overwrite propagation model with Matrix!
  Ptr<MatrixPropagationLossModel> matrix = CreateObject<MatrixPropagationLossModel>();
  matrix->SetDefaultLossDb(200); // isolate by default, except allowed entries
  matrix->SetLoss (nodes.Get(0)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);
  matrix->SetLoss (nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(0)->GetObject<MobilityModel>(), 50);
  matrix->SetLoss (nodes.Get(1)->GetObject<MobilityModel>(), nodes.Get(2)->GetObject<MobilityModel>(), 50);
  matrix->SetLoss (nodes.Get(2)->GetObject<MobilityModel>(), nodes.Get(1)->GetObject<MobilityModel>(), 50);

  chan->SetPropagationLossModel (matrix);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(chan);
  phy.Set("RxGain", DoubleValue(0));
  phy.SetStandard(WIFI_PHY_STANDARD_80211b);

  WifiMacHelper mac;
  // Ad hoc MAC
  mac.SetType("ns3::AdhocWifiMac");

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  // Set RTS/CTS threshold
  if (enableRtsCts)
    {
      Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));
    }
  else
    {
      Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2200));
    }

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // 4. Internet Stack/IPs
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // 5. Applications: Node 0 -> Node 1, Node 2 -> Node 1
  uint16_t port1 = 8000;
  uint16_t port2 = 8001;

  // OnOff - UDP - 512 bytes, 1Mbps data rate, CBR
  OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(interfaces.Get(1), port1));
  onoff1.SetAttribute("PacketSize", UintegerValue(512));
  onoff1.SetAttribute("DataRate", StringValue("1Mbps"));
  onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
  app1.Start(Seconds(1.0));
  app1.Stop(Seconds(10.0));

  OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.Get(1), port2));
  onoff2.SetAttribute("PacketSize", UintegerValue(512));
  onoff2.SetAttribute("DataRate", StringValue("1Mbps"));
  onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
  app2.Start(Seconds(1.1)); // slight stagger
  app2.Stop(Seconds(10.0));

  // Sinks on node 1
  PacketSinkHelper sinkHelper1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
  ApplicationContainer sinkApp1 = sinkHelper1.Install(nodes.Get(1));
  sinkApp1.Start(Seconds(0.0));
  sinkApp1.Stop(Seconds(11.0));

  PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
  ApplicationContainer sinkApp2 = sinkHelper2.Install(nodes.Get(1));
  sinkApp2.Start(Seconds(0.0));
  sinkApp2.Stop(Seconds(11.0));

  // 6. Flow Monitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  PrintFlowStats(monitor, flowHelper);

  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  // Make sure logging is off to keep stats output clean.
  LogComponentDisableAll(LOG_LEVEL_INFO);

  // Run without RTS/CTS
  RunHiddenTerminal(false);

  // Run with RTS/CTS enabled for packets >100B
  RunHiddenTerminal(true);

  return 0;
}