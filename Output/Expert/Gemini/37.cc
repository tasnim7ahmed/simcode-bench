#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>

using namespace ns3;

int main(int argc, char *argv[]) {
  std::string rateControl = "ParfWifiManager";
  uint32_t rtsThreshold = 2347;
  std::string throughputFilename = "throughput.plt";
  std::string txPowerFilename = "txpower.plt";

  CommandLine cmd;
  cmd.AddValue("rateControl", "Rate control algorithm [ParfWifiManager|AparfWifiManager|RrpaaWifiManager]", rateControl);
  cmd.AddValue("rtsThreshold", "RTS/CTS threshold in bytes", rtsThreshold);
  cmd.AddValue("throughputFilename", "Throughput output filename", throughputFilename);
  cmd.AddValue("txPowerFilename", "Transmit power output filename", txPowerFilename);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  wifi.SetRemoteStationManager(rateControl, "RtsCtsThreshold", UintegerValue(rtsThreshold));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifi.Install(phy, mac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes.Get(0));

  Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel>();
  nodes.Get(1)->AggregateObject(staMobility);
  staMobility->SetPosition(Vector(1.0, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(apDevice.Get(0), staDevice.Get(0));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
  client.SetAttribute("Interval", TimeValue(Seconds(0.0000083333)));
  client.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Schedule(Seconds(1.0), [&]() {
    Config::ConnectWithoutContext("/NodeList/1/DeviceList/*/Phy/State/TxPowerDbm", MakeCallback([](double dbm){
        std::cout << Simulator::Now().Seconds() << " " << dbm << std::endl;
    }));
  });

  double distance = 1.0;
  Simulator::Schedule(Seconds(2.0), [&]() {
      Simulator::Schedule(Seconds(1.0), [&, distance]() mutable {
        staMobility->SetPosition(Vector(distance, 0.0, 0.0));
        distance += 1.0;
        if (distance <= 10.0) {
          Simulator::Schedule(Seconds(1.0), std::bind(Simulator::Run));
        }
      });
        Simulator::Run();
  });

  Simulator::Stop(Seconds(11.0));

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  std::ofstream throughputFile, txPowerFile;
  throughputFile.open(throughputFilename);
  txPowerFile.open(txPowerFilename);

  throughputFile << "# Distance (m)  Throughput (Mbps)" << std::endl;
  txPowerFile << "# Distance (m)  Tx Power (dBm)" << std::endl;

  for (auto it = stats.begin(); it != stats.end(); ++it) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
    if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1)) {
      double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000;
      throughputFile << staMobility->GetPosition().x << " " << throughput << std::endl;

       Config::ConnectWithoutContext("/NodeList/1/DeviceList/*/Phy/State/TxPowerDbm", MakeCallback([&](double dbm){
           txPowerFile << staMobility->GetPosition().x << " " << dbm << std::endl;
        }));
    }
  }

  throughputFile.close();
  txPowerFile.close();

  Gnuplot gnuplotThroughput(throughputFilename);
  gnuplotThroughput.AddTitle("Throughput vs. Distance");
  gnuplotThroughput.SetLegend("Distance (m)", "Throughput (Mbps)");
  gnuplotThroughput.AddDataset("", throughputFilename, Gnuplot::STYLE_LINES);
  gnuplotThroughput.GenerateOutput("throughput.png");

    Gnuplot gnuplotTxPower(txPowerFilename);
  gnuplotTxPower.AddTitle("Tx Power vs. Distance");
  gnuplotTxPower.SetLegend("Distance (m)", "TxPower (dBm)");
  gnuplotTxPower.AddDataset("", txPowerFilename, Gnuplot::STYLE_LINES);
  gnuplotTxPower.GenerateOutput("txpower.png");

  Simulator::Destroy();
  return 0;
}