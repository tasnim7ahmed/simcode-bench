#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/data-collection-module.h"
#include <ns3/buildings-helper.h>
#include <ns3/config-store-module.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistanceExperiment");

class ExperimentHelper {
public:
    ExperimentHelper(uint32_t runId, double distance, std::string outputFormat)
        : m_runId(runId), m_distance(distance), m_outputFormat(outputFormat) {}

    void Run();

private:
    uint32_t m_runId;
    double m_distance;
    std::string m_outputFormat;

    void OnTx(Ptr<const Packet> packet);
    void OnRx(Ptr<const Packet> packet, const Address &address);
    void SetupNodes(NodeContainer &nodes);
    void SetupMobility(NodeContainer &nodes);
    void SetupWifi(NodeContainer &nodes, NetDeviceContainer &devices);
    void SetupApplications(NetDeviceContainer &devices, ApplicationContainer &apps);
};

void ExperimentHelper::SetupNodes(NodeContainer &nodes) {
    nodes.Create(2);
}

void ExperimentHelper::SetupMobility(NodeContainer &nodes) {
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(m_distance, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
}

void ExperimentHelper::SetupWifi(NodeContainer &nodes, NetDeviceContainer &devices) {
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211a);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiMac.SetType("ns3::AdhocWifiMac");
    devices = wifiHelper.Install(wifiPhy, wifiMac, nodes);
}

void ExperimentHelper::SetupApplications(NetDeviceContainer &devices, ApplicationContainer &apps) {
    UdpServerHelper server(9);
    apps.Add(server.Install(devices.Get(1)));

    UdpClientHelper client(Ipv4Address("10.1.1.2"), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    apps.Add(client.Install(devices.Get(0)));
}

void ExperimentHelper::OnTx(Ptr<const Packet> packet) {
    NS_LOG_DEBUG("Packet transmitted of size " << packet->GetSize());
    DataCollector::Get()->AddData("tx_packets", 1);
    DataCollector::Get()->AddData("tx_bytes", packet->GetSize());
}

void ExperimentHelper::OnRx(Ptr<const Packet> packet, const Address &address) {
    NS_LOG_DEBUG("Packet received of size " << packet->GetSize());
    DataCollector::Get()->AddData("rx_packets", 1);
    DataCollector::Get()->AddData("rx_bytes", packet->GetSize());
}

void ExperimentHelper::Run() {
    NodeContainer nodes;
    NetDeviceContainer devices;
    ApplicationContainer apps;

    SetupNodes(nodes);
    SetupMobility(nodes);
    SetupWifi(nodes, devices);
    SetupApplications(devices, apps);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&ExperimentHelper::OnTx, this));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&ExperimentHelper::OnRx, this));

    DataCollector dataCollector;
    dataCollector.Start(Time::FromDouble(0, Time::S));
    dataCollector.Stop(Time::FromDouble(10, Time::S));

    dataCollector.AddMetadata("run_id", std::to_string(m_runId));
    dataCollector.AddMetadata("distance", std::to_string(m_distance));

    dataCollector.AddData("tx_packets", 0);
    dataCollector.AddData("tx_bytes", 0);
    dataCollector.AddData("rx_packets", 0);
    dataCollector.AddData("rx_bytes", 0);

    if (m_outputFormat == "omnet") {
        OmnetDataOutput omnetOutput;
        omnetOutput.Output(dataCollector);
    } else if (m_outputFormat == "sqlite") {
        SqliteDataOutput sqliteOutput;
        sqliteOutput.SetOutputFilename("wifi_experiment.sqlite");
        sqliteOutput.Output(dataCollector);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    uint32_t runId = 1;
    double distance = 100.0;
    std::string outputFormat = "omnet";

    CommandLine cmd(__FILE__);
    cmd.AddValue("runId", "Run identifier for the experiment", runId);
    cmd.AddValue("distance", "Distance between two nodes in meters", distance);
    cmd.AddValue("format", "Output format: 'omnet' or 'sqlite'", outputFormat);
    cmd.Parse(argc, argv);

    ExperimentHelper helper(runId, distance, outputFormat);
    helper.Run();

    return 0;
}