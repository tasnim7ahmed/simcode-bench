#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateAdaptation");

int main(int argc, char *argv[]) {
  bool enableMinstrel = true;
  bool enableLogging = false;
  std::string phyMode("HtMcs7");
  double distanceStep = 1.0; // meters
  double timeStep = 1.0;     // seconds
  double simulationTime = 10.0; // seconds
  std::string csvFileName = "wifi-rate-adaptation.csv";

  CommandLine cmd;
  cmd.AddValue("enableMinstrel", "Enable Minstrel rate adaptation", enableMinstrel);
  cmd.AddValue("enableLogging", "Enable logging of rate changes", enableLogging);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("distanceStep", "Distance step (meters)", distanceStep);
  cmd.AddValue("timeStep", "Time step (seconds)", timeStep);
  cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
  cmd.AddValue("csvFileName", "CSV output file name", csvFileName);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

  if (enableMinstrel) {
    // Configure Minstrel on the STA
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ এড",
                StringValue("ns3::MinstrelHtWifiManager"));
  }

  if (enableLogging) {
    // Enable logging of rate changes (example: log to console)
    // Replace with file logging if needed.
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ এড/Minstrel/ChangeRate",
                   MakeCallback([](std::string path, WifiMode newRate) {
                     std::cout << Simulator::Now().Seconds() << " " << path << " changed to " << newRate << std::endl;
                   }));
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes.Get(0)); // AP at (0,0)

  Ptr<ConstantPositionMobilityModel> staMobility = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  staMobility->SetPosition(Vector(10.0, 0.0, 0.0)); // Initial STA position

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(apDevices);
  ipv4.Assign(staDevices);

  UdpServerHelper server(9);
  ApplicationContainer apps = server.Install(nodes.Get(1));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client(i.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.000002135s"))); // CBR 400 Mbps
  client.SetAttribute("PacketSize", UintegerValue(1024));

  apps = client.Install(nodes.Get(0));
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Create a CSV file for throughput vs distance
  std::ofstream csvFile;
  csvFile.open(csvFileName);
  csvFile << "Time(s),Distance(m),Throughput(Mbps)\n";

  // Run simulation with varying distances
  for (double time = timeStep; time <= simulationTime; time += timeStep) {
    double distance = 10.0 + distanceStep * (time / timeStep); // Move STA away
    staMobility->SetPosition(Vector(distance, 0.0, 0.0));
    Simulator::Run(Seconds(time));

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    double throughput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.sourceAddress == i.GetAddress(1) && t.destinationAddress == i.GetAddress(0)) {
        continue; // Skip reverse flow
      }
      throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
    }
    csvFile << time << "," << distance << "," << throughput << "\n";
    monitor->SerializeToXmlFile("wifi-rate-adaptation.flowmon", true, true);
  }
  csvFile.close();

  Simulator::Destroy();

  // Generate Gnuplot file
  Gnuplot plot("throughput_vs_distance.png");
  plot.SetTitle("Wi-Fi Throughput vs Distance");
  plot.SetLegend("Distance (m)", "Throughput (Mbps)");
  plot.AddDataset(csvFileName, "2:3", "linespoints"); // x:Distance, y: Throughput
  plot.GenerateOutput();

  return 0;
}