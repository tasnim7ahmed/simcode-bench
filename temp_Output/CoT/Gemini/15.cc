#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoSimulation");

int main(int argc, char *argv[]) {
  bool verbose = false;
  bool tracing = false;
  std::string protocol = "Udp"; // Udp or Tcp
  double simulationTime = 10;   // Seconds
  double stepSize = 10;
  bool channelBonding = false;
  std::string frequencyBand = "2.4GHz"; // 2.4GHz or 5.0GHz

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if true", verbose);
  cmd.AddValue("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue("protocol", "Transport protocol to use: Udp or Tcp", protocol);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("stepSize", "Distance step size in meters", stepSize);
  cmd.AddValue("channelBonding", "Enable 40 MHz channel bonding", channelBonding);
  cmd.AddValue("frequencyBand", "Frequency Band (2.4GHz or 5.0GHz)", frequencyBand);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("WifiMimoSimulation", LOG_LEVEL_INFO);
  }

  NodeContainer apNode;
  apNode.Create(1);
  NodeContainer staNode;
  staNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  mobility.Install(staNode);
  Ptr<ConstantPositionMobilityModel> staMob = staNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
  staMob->SetPosition(Vector(10.0, 0.0, 0.0));

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  ApplicationContainer app;
  if (protocol == "Udp") {
    UdpEchoServerHelper echoServer(port);
    app = echoServer.Install(apNode);
    app.Start(Seconds(0.0));
    app.Stop(Seconds(simulationTime + 1));
  }

  double distance = 10.0;
  std::map<int, std::vector<double>> throughputData;

  for (int streams = 1; streams <= 4; ++streams) {
    for (int mcs = 0; mcs <= 7; ++mcs) {
      throughputData[streams * 10 + mcs] = std::vector<double>();
    }
  }

  while (distance <= 100) {
    staMob->SetPosition(Vector(distance, 0.0, 0.0));
    Simulator::Stop(Seconds(0.1));
    Simulator::Run();

    for (int streams = 1; streams <= 4; ++streams) {
      for (int mcs = 0; mcs <= 7; ++mcs) {
        Config::Set("/NodeList/*/$ns3::WifiNetDevice/HtConfiguration/AmpduParameters/MpduDensity",EnumValue(streams));
        Config::Set("/NodeList/*/$ns3::WifiNetDevice/HtConfiguration/ControlMcs",UintegerValue(mcs));

        uint32_t packetSize = 1472;
        uint32_t maxPackets = (uint32_t)((simulationTime*1000000)/100);
        Time interPacketInterval = MicroSeconds(100);

        ApplicationContainer clientApp;

        if (protocol == "Udp") {
          UdpEchoClientHelper echoClient(apInterface.GetAddress(0), port);
          echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
          echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
          echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
          clientApp = echoClient.Install(staNode);
        } else {
           BulkSendHelper source("ns3::TcpSocketFactory",InetSocketAddress(apInterface.GetAddress(0), port));
           source.SetAttribute("MaxBytes", UintegerValue(maxPackets * packetSize));
           clientApp = source.Install(staNode);

           PacketSinkHelper sink ("ns3::TcpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(), port));
           ApplicationContainer sinkApp = sink.Install(apNode);
           sinkApp.Start(Seconds(0.0));
           sinkApp.Stop(Seconds(simulationTime + 1));
        }

        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));

        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        Simulator::Stop(Seconds(simulationTime + 2));
        Simulator::Run();

        monitor->CheckForLostPackets ();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
        double throughput = 0.0;
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
          {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
            if (t.sourceAddress == staInterface.GetAddress(0) && t.destinationAddress == apInterface.GetAddress(0))
              {
                throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000/ 1000;
              }
          }
        throughputData[streams * 10 + mcs].push_back(throughput);
        monitor->SerializeToXmlFile("mimo-simulation.flowmon", true, true);
        Simulator::Destroy();
      }
    }
    distance += stepSize;
  }

  Gnuplot2dDataset dataset;
  Gnuplot plot("mimo-throughput.plt");
  plot.SetTitle("802.11n MIMO Throughput vs Distance");
  plot.SetLegend("MIMO Streams", "Throughput (Mbps)");
  plot.AddDataset(dataset);
  plot.SetTerminal("png");
  plot.SetOutput("mimo-throughput.png");
  plot.SetXRange(0, 100);
  plot.SetYRange(0, 100);
  plot.SetExtra("set key autotitle columnhead");

  for (int streams = 1; streams <= 4; ++streams) {
    for (int mcs = 0; mcs <= 7; ++mcs) {
        Gnuplot2dDataset dataset2;
        dataset2.SetTitle(std::to_string(streams) + " Streams, MCS " + std::to_string(mcs));
        int i = 0;
        distance = 10;

        for(auto const& value : throughputData[streams * 10 + mcs]) {
            dataset2.Add(distance, value);
            distance += stepSize;
        }
        plot.AddDataset(dataset2);

    }
  }
  plot.GenerateOutput();

  if (tracing) {
    phy.EnablePcapAll("wifi-mimo");
  }

  return 0;
}