#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/config.h"
#include "ns3/gnuplot.h"
#include "ns3/wifi-phy.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateComparison");

int main(int argc, char *argv[]) {
  bool verbose = false;
  std::string rateControl = "AparfWifiManager";
  double simulationTime = 10;
  double distance = 10;
  uint16_t port = 9;
  std::string rtsThreshold = "2347";
  std::string throughputFilename = "throughput.dat";
  std::string powerFilename = "power.dat";

  CommandLine cmd;
  cmd.AddValue("verbose", "Tell application to log if set.", verbose);
  cmd.AddValue("rateControl", "Rate control algorithm", rateControl);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Initial distance in meters", distance);
  cmd.AddValue("port", "Port number", port);
  cmd.AddValue("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
  cmd.AddValue("throughputFilename", "Throughput output filename", throughputFilename);
  cmd.AddValue("powerFilename", "Power output filename", powerFilename);
  cmd.Parse(argc, argv);

  if (verbose) {
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
  }

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(rtsThreshold));

  wifi.SetRemoteStationManager(rateControl, "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid), "BeaconGeneration", BooleanValue(true));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(distance), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(0.0), "DeltaY", DoubleValue(0.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes.Get(0));

  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(distance), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(0.0), "DeltaY", DoubleValue(0.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::LinearMobilityModel", "Velocity", StringValue("0.0 -1.0 0.0"));
  mobility.Install(nodes.Get(1));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  address.Assign(apDevices);

  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simulationTime + 1));

  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("MaxPackets", UintegerValue(4294967295));
  client.SetAttribute("Interval", TimeValue(Time("0.000001")));
  client.SetAttribute("PacketSize", UintegerValue(1472));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(simulationTime + 1));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Stop(Seconds(simulationTime + 1));

  Ptr<OutputStreamWrapper> throughputStream = Create<OutputStreamWrapper>(throughputFilename, std::ios::out);
  Ptr<OutputStreamWrapper> powerStream = Create<OutputStreamWrapper>(powerFilename, std::ios::out);

  Simulator::Schedule(Seconds(1.1), [&, throughputStream, powerStream]() {
        *throughputStream->GetStream() << "#Time Throughput(bps)" << std::endl;
        *powerStream->GetStream() << "#Time Power(dBm)" << std::endl;
    });
    
  Simulator::Schedule(Seconds(1.1), [&, throughputStream, powerStream, monitor, simulationTime, apDevices]() {
    for(double t = 1.1; t <= simulationTime; t += 1.0) {
      Simulator::Schedule(Seconds(t), [&, throughputStream, powerStream, monitor, apDevices, t]() {
        monitor->CheckForLostPackets();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        double throughputSum = 0;
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
          if (t.destinationAddress == interfaces.GetAddress(1)) {
            throughputSum += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds());
          }
        }
        *throughputStream->GetStream() << t << " " << throughputSum << std::endl;

        Ptr<WifiPhy> phyAp = DynamicCast<WifiPhy>(apDevices.Get(0)->GetChannel()->GetPhy(0));
        double txPower = phyAp->GetTxPowerDbm();
        *powerStream->GetStream() << t << " " << txPower << std::endl;
      });
    }
  });

  Simulator::Run();

  monitor->SerializeToXmlFile("wifi_rate_comparison.xml", true, true);

  Simulator::Destroy();

  return 0;
}