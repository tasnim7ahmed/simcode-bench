#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211nMimo");

int main(int argc, char *argv[]) {
  // Enable logging
  LogComponentEnable("Wifi80211nMimo", LOG_LEVEL_INFO);

  // Set default values for parameters
  double simulationTime = 10.0; // seconds
  double distanceStep = 10.0;   // meters
  double maxDistance = 100.0;  // meters
  bool useTcp = false;            // Use UDP by default
  std::string frequencyBand = "2.4GHz"; // Default frequency band
  int channelWidth = 20; // MHz
  bool shortGuardInterval = false;
  bool preambleDetection = true;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distanceStep", "Distance step in meters", distanceStep);
  cmd.AddValue("maxDistance", "Maximum distance in meters", maxDistance);
  cmd.AddValue("useTcp", "Use TCP instead of UDP (0=UDP, 1=TCP)", useTcp);
  cmd.AddValue("frequencyBand", "Frequency band (2.4GHz or 5.0GHz)", frequencyBand);
  cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
  cmd.AddValue("shortGuardInterval", "Enable short guard interval (0=false, 1=true)", shortGuardInterval);
  cmd.AddValue("preambleDetection", "Enable preamble detection (0=false, 1=true)", preambleDetection);

  cmd.Parse(argc, argv);

  // Validate frequency band
  if (frequencyBand != "2.4GHz" && frequencyBand != "5.0GHz") {
    std::cerr << "Error: Invalid frequency band. Must be 2.4GHz or 5.0GHz." << std::endl;
    return 1;
  }

  // Create nodes: an access point (AP) and a station (STA)
  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNode;
  staNode.Create(1);

  // Configure Wi-Fi PHY
  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();

  if (frequencyBand == "2.4GHz") {
     phyHelper.Set("ChannelNumber", UintegerValue(1));
  } else {
     phyHelper.Set("ChannelNumber", UintegerValue(36));
  }

  phyHelper.Set("ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));

  channelHelper.AddPropagationLoss("ns3::FriisPropagationLossModel");
  channelHelper.AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  phyHelper.SetChannel(channelHelper.Create());

  // Configure Wi-Fi MAC
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper macHelper = NqosWifiMacHelper::Default();

  Ssid ssid = Ssid("ns-3-ssid");
  macHelper.SetType("ns3::ApWifiMac",
                   "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

  macHelper.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(ssid),
                   "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevice = wifiHelper.Install(phyHelper, macHelper, staNode);

  // Configure mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNode);

  // Install Internet stack
  InternetStackHelper stackHelper;
  stackHelper.Install(apNode);
  stackHelper.Install(staNode);

  Ipv4AddressHelper addressHelper;
  addressHelper.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = addressHelper.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = addressHelper.Assign(staDevice);

  // Create Gnuplot output file
  std::string fileNameWithExtension = "wifi-80211n-mimo.plt";
  std::string graphicsFileNameWithExtension = "wifi-80211n-mimo.eps";
  std::string plotTitle = "802.11n MIMO Throughput vs. Distance";

  Gnuplot gnuplot(fileNameWithExtension);
  gnuplot.SetTitle(plotTitle);
  gnuplot.SetLegend("Distance (m)", "Throughput (Mbps)");

  // Configure Gnuplot output
  gnuplot.AddDatasetTitle("MIMO Throughput");

  // Loop through MCS values and distances
  for (int mimoStreams = 1; mimoStreams <= 4; ++mimoStreams) {
    for (int mcs = 0; mcs < 8; ++mcs) { // MCS values for each stream
      std::stringstream ss;
      ss << "HtMcs" << mcs;
      Config::Set ("/NodeList/*/$ns3::WifiNetDevice/HtConfiguration/SupportedHtMcs", StringValue (ss.str ()));

      std::vector<std::pair<double, double>> dataPoints; // Store distance and throughput

      for (double distance = distanceStep; distance <= maxDistance; distance += distanceStep) {
        // Set STA position
        Ptr<Node> staNodePtr = staNode.Get(0);
        Ptr<MobilityModel> staMobility = staNodePtr->GetObject<MobilityModel>();
        staMobility->SetPosition(Vector(distance, 0.0, 0.0));

        // Create traffic flow
        uint16_t port = 50000;
        ApplicationContainer app;
        if (useTcp) {
          // TCP traffic
            BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
            source.SetAttribute("MaxBytes", UintegerValue(0));
            app = source.Install(staNode);

            PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            app.Add(sink.Install(apNode));
        } else {
          // UDP traffic
          UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
          clientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295));
          clientHelper.SetAttribute("Interval", TimeValue(Time("0.00002"))); // packets/sec
          clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
          app = clientHelper.Install(staNode);

          UdpServerHelper serverHelper(port);
          app.Add(serverHelper.Install(apNode));
        }

        app.Start(Seconds(0.1));
        app.Stop(Seconds(simulationTime));

        // Flow monitor
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        // Run simulation
        Simulator::Stop(Seconds(simulationTime + 0.1));
        Simulator::Run();

        // Calculate throughput
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        double throughput = 0.0;
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
          if (t.sourceAddress == staInterface.GetAddress(0) && t.destinationAddress == apInterface.GetAddress(0)) {
            throughput = i->second.rxBytes * 8.0 / (simulationTime * 1000000.0); // Mbps
            break;
          }
        }

        dataPoints.push_back(std::make_pair(distance, throughput));

        monitor->SerializeToXmlFile("wifi-80211n-mimo.flowmon", false, true);

        Simulator::Destroy();
      }

      // Add data to Gnuplot
      std::stringstream datasetTitleStream;
      datasetTitleStream << "MIMO Streams: " << mimoStreams << ", MCS: " << mcs;
      gnuplot.AddDataset(dataPoints, datasetTitleStream.str());
    }
  }

  // Generate plot
  gnuplot.GenerateOutput(graphicsFileNameWithExtension);

  std::cout << "Simulation finished. Plot generated: " << graphicsFileNameWithExtension << std::endl;

  return 0;
}