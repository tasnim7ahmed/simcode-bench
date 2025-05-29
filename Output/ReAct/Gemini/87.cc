#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/data-calculator.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace ns3;

class DataCollector {
public:
  DataCollector(std::string experimentName, std::string format, std::string run)
    : m_experimentName(experimentName),
      m_format(format),
      m_run(run)
  {
  }

  void AddData(std::string key, std::string value)
  {
    m_data[key] = value;
  }

  void WriteData()
  {
    if (m_format == "omnet") {
      WriteOmnet();
    } else if (m_format == "sqlite") {
      WriteSqlite();
    } else {
      std::cerr << "Error: Unknown format " << m_format << std::endl;
    }
  }

private:
  void WriteOmnet()
  {
    std::ofstream outFile;
    outFile.open(m_experimentName + "-" + m_run + ".sca");
    outFile << "attr experiment " << m_experimentName << std::endl;
    outFile << "attr run " << m_run << std::endl;

    for (auto const& [key, val] : m_data) {
      outFile << "scalar " << m_experimentName << " " << key << " " << val << std::endl;
    }

    outFile.close();
  }

  void WriteSqlite()
  {
    std::cerr << "Sqlite output not implemented yet." << std::endl;
  }

  std::string m_experimentName;
  std::string m_format;
  std::string m_run;
  std::map<std::string, std::string> m_data;
};

int main(int argc, char *argv[]) {
  double distance = 10.0;
  std::string format = "omnet";
  std::string run = "1";
  std::string phyMode("DsssRate1Mbps");
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("format", "Output format (omnet, sqlite)", format);
  cmd.AddValue("run", "Run identifier", run);
  cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(1023));
  Config::SetDefault("ns3::WifiMac::BE_BlockAckThreshold", UintegerValue(1));
  Config::SetDefault("ns3::WifiMac::BE_BlockAckInactivityTimeout", UintegerValue(200));

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  AdhocWifiMacHelper wifiMac;
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac.Install(nodes));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign(devices);

  UdpEchoServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(i.GetAddress(1), 9);
  echoClient.SetAttribute("MaxPackets", UintegerValue(100000));
  echoClient.SetAttribute("Interval", TimeValue(MicroSeconds(10)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));

  DataCollector dataCollector("wifi-distance", format, run);

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (auto i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if (t.sourceAddress == "10.1.1.1" && t.destinationAddress == "10.1.1.2")
        {
          dataCollector.AddData("packetsSent", std::to_string(i->second.txPackets));
          dataCollector.AddData("packetsReceived", std::to_string(i->second.rxPackets));
          dataCollector.AddData("bytesSent", std::to_string(i->second.txBytes));
          dataCollector.AddData("bytesReceived", std::to_string(i->second.rxBytes));
          dataCollector.AddData("delaySum", std::to_string(i->second.delaySum.GetSeconds()));
          dataCollector.AddData("lostPackets", std::to_string(i->second.lostPackets));
        }
    }

  dataCollector.AddData("distance", std::to_string(distance));
  dataCollector.WriteData();

  Simulator::Destroy();
  return 0;
}