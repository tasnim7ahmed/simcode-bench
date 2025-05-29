#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistanceExperiment");

// DataCollector class for experiment results
class DataCollector {
public:
    DataCollector(std::string format, std::string runId) : format_(format), runId_(runId) {}

    void AddTransmission(Time time, uint32_t packetSize) {
        txPackets_++;
        txBytes_ += packetSize;
        txTimes_.push_back(time.GetSeconds());
    }

    void AddReception(Time time, uint32_t packetSize) {
        rxPackets_++;
        rxBytes_ += packetSize;
        rxTimes_.push_back(time.GetSeconds());
    }

    void AddDelay(Time delay) {
        totalDelay_ += delay;
        delayCount_++;
    }

    void Report() {
        if (format_ == "omnet") {
            ReportOmnet();
        } else if (format_ == "sqlite") {
            ReportSqlite();
        } else {
            std::cerr << "Unknown format: " << format_ << std::endl;
        }
    }

private:
    void ReportOmnet() {
        std::ofstream outFile(runId_ + ".sca");
        outFile << "attr experiment " << runId_ << std::endl;
        outFile << "scalar WifiDistanceExperiment txPackets " << txPackets_ << std::endl;
        outFile << "scalar WifiDistanceExperiment rxPackets " << rxPackets_ << std::endl;
        outFile << "scalar WifiDistanceExperiment txBytes " << txBytes_ << std::endl;
        outFile << "scalar WifiDistanceExperiment rxBytes " << rxBytes_ << std::endl;
        outFile << "scalar WifiDistanceExperiment averageDelay " << (delayCount_ > 0 ? totalDelay_.GetSeconds() / delayCount_ : 0) << std::endl;
        outFile.close();
    }

    void ReportSqlite() {
      std::ofstream outFile(runId_ + ".sql");
      outFile << "CREATE TABLE IF NOT EXISTS results (runId TEXT, txPackets INTEGER, rxPackets INTEGER, txBytes INTEGER, rxBytes INTEGER, averageDelay REAL);" << std::endl;
      outFile << "INSERT INTO results VALUES ('" << runId_ << "', " << txPackets_ << ", " << rxPackets_ << ", " << txBytes_ << ", " << rxBytes_ << ", " << (delayCount_ > 0 ? totalDelay_.GetSeconds() / delayCount_ : 0) << ");" << std::endl;
      outFile.close();
    }

    std::string format_;
    std::string runId_;
    uint32_t txPackets_ = 0;
    uint32_t rxPackets_ = 0;
    uint64_t txBytes_ = 0;
    uint64_t rxBytes_ = 0;
    Time totalDelay_ = Seconds(0);
    uint32_t delayCount_ = 0;
    std::vector<double> txTimes_;
    std::vector<double> rxTimes_;
};

// Global DataCollector instance
DataCollector* dataCollector;

// Callback for packet transmission
void PacketTx(Time time, Ptr<Packet const> packet) {
    dataCollector->AddTransmission(time, packet->GetSize());
}

// Callback for packet reception
void PacketRx(Time time, Ptr<Packet const> packet) {
    dataCollector->AddReception(time, packet->GetSize());
}

// Callback for end-to-end delay measurement
void PacketDelay(Time delay) {
    dataCollector->AddDelay(delay);
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    double distance = 10.0; // Default distance
    std::string format = "omnet"; // Default format
    std::string runId = "default"; // Default runId

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log ifconfig interface output", verbose);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("format", "Output format (omnet or sqlite)", format);
    cmd.AddValue("runId", "Run identifier", runId);
    cmd.Parse(argc, argv);

    dataCollector = new DataCollector(format, runId);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(1024);)

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up tracing for packet transmissions and receptions
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpClient/Tx", MakeCallback(&PacketTx));
    Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback(&PacketRx));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      Time delay = Seconds(i->second.delaySum.GetSeconds() / i->second.rxPackets);
      PacketDelay(delay);
    }
    dataCollector->Report();

    Simulator::Destroy();

    delete dataCollector;

    return 0;
}